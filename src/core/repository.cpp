#include "light_git/repository.hpp"
#include "light_git/filesystem.hpp"
#include "light_git/crypto.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <set>
#include <queue>
#include <map>
#include <algorithm>

namespace light_git {

Repository::Repository(std::filesystem::path work_dir, std::filesystem::path git_dir, bool bare)
    : work_dir_(std::move(work_dir)), git_dir_(std::move(git_dir)), is_bare_(bare) {
    objects_ = std::make_unique<ObjectStore>(git_dir_ / "objects");
    refs_ = std::make_unique<References>(git_dir_);
    index_ = std::make_unique<Index>();
    config_ = std::make_unique<RepoConfig>();
}

std::optional<std::filesystem::path> Repository::find_repository_root(const std::filesystem::path& start_path) {
    std::filesystem::path cur = std::filesystem::absolute(start_path);
    while (true) {
        if (std::filesystem::exists(cur / ".light-git")) {
            return cur;
        }
        // Bare repo check (e.g. current directory itself is bare git repo)
        if (std::filesystem::exists(cur / "HEAD") && std::filesystem::exists(cur / "objects") &&
            std::filesystem::exists(cur / "refs")) {
            return cur;
        }
        if (!cur.has_parent_path() || cur == cur.parent_path()) {
            break;
        }
        cur = cur.parent_path();
    }
    return std::nullopt;
}

Repository Repository::init(const std::filesystem::path& path, bool bare, const std::string& default_branch) {
    std::filesystem::path work_dir;
    std::filesystem::path git_dir;

    if (bare) {
        work_dir = path;
        git_dir = path;
    } else {
        work_dir = path;
        git_dir = path / ".light-git";
    }

    std::filesystem::create_directories(git_dir / "objects");
    std::filesystem::create_directories(git_dir / "refs" / "heads");
    std::filesystem::create_directories(git_dir / "refs" / "tags");

    // Write HEAD
    std::string head_content = "ref: refs/heads/" + default_branch + "\n";
    fs_util::atomic_write_file(git_dir / "HEAD", head_content);

    // Write initial config
    std::string config_content = "[core]\n\tbare = " + std::string(bare ? "true" : "false") +
                                 "\n\tignorecase = true\n[init]\n\tdefaultBranch = " + default_branch + "\n";
    fs_util::atomic_write_file(git_dir / "config", config_content);

    Repository repo(work_dir, git_dir, bare);
    repo.config_->load(git_dir / "config");
    repo.index_->load_from_file(git_dir / "index");
    repo.load_ignores();

    return repo;
}

Repository Repository::open(const std::filesystem::path& start_path) {
    auto root_opt = find_repository_root(start_path);
    if (!root_opt) {
        throw std::runtime_error("Not a light-git repository (or any of the parent directories): .light-git");
    }

    std::filesystem::path root = *root_opt;
    bool bare = !std::filesystem::exists(root / ".light-git");
    std::filesystem::path work_dir = root;
    std::filesystem::path git_dir = bare ? root : (root / ".light-git");

    Repository repo(work_dir, git_dir, bare);
    repo.config_->load(git_dir / "config");
    repo.index_->load_from_file(git_dir / "index");
    repo.load_ignores();

    return repo;
}

void Repository::load_ignores() {
    ignores_.clear();
    if (!is_bare_) {
        std::filesystem::path ig_path = work_dir_ / ".light-gitignore";
        if (std::filesystem::exists(ig_path)) {
            ignores_.load_from_file(ig_path);
        }
    }
}

namespace {
struct TreeNode {
    std::string name;
    std::map<std::string, IndexEntry> files;
    std::map<std::string, TreeNode> subdirs;
};

ObjectId build_tree_recursive(ObjectStore& store, const TreeNode& node) {
    Tree tree;

    for (const auto& [fname, entry] : node.files) {
        TreeEntry te;
        te.mode = entry.mode;
        te.type = ObjectType::Blob;
        te.name = fname;
        te.id = entry.id;
        tree.add_entry(std::move(te));
    }

    for (const auto& [dname, subnode] : node.subdirs) {
        ObjectId sub_id = build_tree_recursive(store, subnode);
        TreeEntry te;
        te.mode = 0040000;
        te.type = ObjectType::Tree;
        te.name = dname;
        te.id = sub_id;
        tree.add_entry(std::move(te));
    }

    std::vector<uint8_t> serialized = tree.serialize();
    return store.write_object(ObjectType::Tree, serialized);
}
} // namespace

ObjectId Repository::write_tree_from_index() {
    if (index_->entries().empty()) {
        Tree empty_tree;
        return objects_->write_object(ObjectType::Tree, empty_tree.serialize());
    }

    TreeNode root;
    for (const auto& [path, entry] : index_->entries()) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start < path.size()) {
            size_t slash = path.find('/', start);
            if (slash == std::string::npos) {
                parts.push_back(path.substr(start));
                break;
            }
            parts.push_back(path.substr(start, slash - start));
            start = slash + 1;
        }

        TreeNode* cur = &root;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            cur = &cur->subdirs[parts[i]];
        }
        cur->files[parts.back()] = entry;
    }

    return build_tree_recursive(*objects_, root);
}

RepoStatus Repository::compute_status() {
    RepoStatus status;
    status.is_detached = refs_->is_head_detached();
    status.branch_name = status.is_detached ? "detached HEAD" : refs_->get_current_branch();
    status.head_commit = refs_->resolve_head();

    load_ignores();

    // 1. Flatten HEAD tree
    std::map<std::string, TreeEntry> head_files;
    if (status.head_commit) {
        try {
            RawObject cobj = objects_->read_object(status.head_commit);
            if (cobj.type == ObjectType::Commit) {
                Commit commit = Commit::deserialize(cobj.data);
                head_files = Tree::flatten_tree(*objects_, commit.tree_id);
            }
        } catch (...) {
            // Commit corrupted or unreadable
        }
    }

    const auto& idx_entries = index_->entries();

    // 2. Staged changes: index vs HEAD
    for (const auto& [path, entry] : idx_entries) {
        auto it = head_files.find(path);
        if (it == head_files.end()) {
            status.staged_new.push_back(path);
        } else if (it->second.id != entry.id || it->second.mode != entry.mode) {
            status.staged_modified.push_back(path);
        }
    }

    for (const auto& [path, entry] : head_files) {
        if (idx_entries.find(path) == idx_entries.end()) {
            status.staged_deleted.push_back(path);
        }
    }

    // 3. Unstaged changes & untracked files: working tree vs index
    if (!is_bare_) {
        auto disk_entries = fs_util::list_directory_recursive(work_dir_);
        std::unordered_set<std::string> on_disk_files;

        for (const auto& de : disk_entries) {
            if (de.is_directory) continue;
            std::string rel_path = fs_util::normalize_path(de.relative_path);
            on_disk_files.insert(rel_path);

            auto it = idx_entries.find(rel_path);
            if (it != idx_entries.end()) {
                const auto& ie = it->second;
                // Fast path: if size and mtime match, assume clean
                if (de.size != ie.size || de.mtime_ms != ie.mtime_ms) {
                    try {
                        std::vector<uint8_t> data = fs_util::read_file_bytes(de.full_path);
                        ObjectId computed_id = hash_bytes(data.data(), data.size());
                        if (computed_id != ie.id) {
                            status.unstaged_modified.push_back(rel_path);
                        }
                    } catch (...) {
                        status.unstaged_modified.push_back(rel_path);
                    }
                }
            } else {
                // Untracked: check ignore matcher
                if (!ignores_.is_ignored(rel_path, false)) {
                    status.untracked.push_back(rel_path);
                }
            }
        }

        // Check for deleted files (in index but not on disk)
        for (const auto& [path, entry] : idx_entries) {
            if (on_disk_files.find(path) == on_disk_files.end()) {
                status.unstaged_deleted.push_back(path);
            }
        }
    }

    std::sort(status.staged_new.begin(), status.staged_new.end());
    std::sort(status.staged_modified.begin(), status.staged_modified.end());
    std::sort(status.staged_deleted.begin(), status.staged_deleted.end());
    std::sort(status.unstaged_modified.begin(), status.unstaged_modified.end());
    std::sort(status.unstaged_deleted.begin(), status.unstaged_deleted.end());
    std::sort(status.untracked.begin(), status.untracked.end());

    return status;
}

void Repository::stage_path(const std::string& rel_or_abs_path) {
    if (is_bare_) {
        throw std::runtime_error("Cannot stage files in a bare repository");
    }

    load_ignores();

    std::filesystem::path target = std::filesystem::absolute(work_dir_ / rel_or_abs_path);
    std::string rel_path;
    try {
        rel_path = fs_util::to_repo_relative(work_dir_, target);
    } catch (...) {
        throw std::runtime_error("Path '" + rel_or_abs_path + "' is outside repository working tree");
    }

    if (rel_path == "." || rel_path.empty()) {
        stage_all();
        return;
    }

    if (std::filesystem::is_directory(target)) {
        auto entries = fs_util::list_directory_recursive(target);
        for (const auto& entry : entries) {
            if (entry.is_directory) continue;
            std::string rpath = fs_util::normalize_path(entry.full_path.lexically_relative(work_dir_));
            if (!ignores_.is_ignored(rpath, false)) {
                std::vector<uint8_t> data = fs_util::read_file_bytes(entry.full_path);
                ObjectId id = objects_->write_object(ObjectType::Blob, data);
                IndexEntry ie;
                ie.path = rpath;
                ie.id = id;
                ie.mode = fs_util::get_file_mode(entry.full_path);
                ie.size = entry.size;
                ie.mtime_ms = entry.mtime_ms;
                index_->add_or_update(std::move(ie));
            }
        }
        index_->save_to_file(git_dir_ / "index");
        return;
    }

    if (std::filesystem::exists(target)) {
        std::vector<uint8_t> data = fs_util::read_file_bytes(target);
        ObjectId id = objects_->write_object(ObjectType::Blob, data);

        IndexEntry ie;
        ie.path = rel_path;
        ie.id = id;
        ie.mode = fs_util::get_file_mode(target);
        ie.size = std::filesystem::file_size(target);
        ie.mtime_ms = fs_util::get_mtime_ms(target);
        index_->add_or_update(std::move(ie));
        index_->save_to_file(git_dir_ / "index");
        return;
    }

    // Path does not exist on disk: check if it's in index to stage deletion
    if (index_->find(rel_path) != nullptr) {
        index_->remove(rel_path);
        index_->save_to_file(git_dir_ / "index");
        return;
    }

    throw std::runtime_error("pathspec '" + rel_or_abs_path + "' did not match any files");
}

void Repository::stage_all() {
    if (is_bare_) return;

    load_ignores();
    auto entries = fs_util::list_directory_recursive(work_dir_);
    std::unordered_set<std::string> on_disk;

    for (const auto& entry : entries) {
        if (entry.is_directory) continue;
        std::string rpath = fs_util::normalize_path(entry.relative_path);
        on_disk.insert(rpath);

        if (!ignores_.is_ignored(rpath, false)) {
            std::vector<uint8_t> data = fs_util::read_file_bytes(entry.full_path);
            ObjectId id = objects_->write_object(ObjectType::Blob, data);

            IndexEntry ie;
            ie.path = rpath;
            ie.id = id;
            ie.mode = fs_util::get_file_mode(entry.full_path);
            ie.size = entry.size;
            ie.mtime_ms = entry.mtime_ms;
            index_->add_or_update(std::move(ie));
        }
    }

    // Stage deletions for missing tracked files
    std::vector<std::string> to_remove;
    for (const auto& [path, ie] : index_->entries()) {
        if (on_disk.find(path) == on_disk.end()) {
            to_remove.push_back(path);
        }
    }
    for (const auto& path : to_remove) {
        index_->remove(path);
    }

    index_->save_to_file(git_dir_ / "index");
}

ObjectId Repository::create_commit(const std::string& message, bool amend) {
    std::string user_name = config_->get_or("user.name", "");
    std::string user_email = config_->get_or("user.email", "");

    if (user_name.empty() || user_email.empty()) {
        throw std::runtime_error("Author identity unknown. Please set your name and email:\n"
                                 "  lgit config user.name \"Your Name\"\n"
                                 "  lgit config user.email \"you@example.com\"");
    }

    ObjectId root_tree = write_tree_from_index();
    ObjectId head_commit = refs_->resolve_head();

    std::vector<ObjectId> parents;
    if (amend) {
        if (!head_commit) {
            throw std::runtime_error("Cannot amend: no existing commit on current branch");
        }
        RawObject head_obj = objects_->read_object(head_commit);
        Commit prev = Commit::deserialize(head_obj.data);
        parents = prev.parents;
    } else {
        if (head_commit) {
            parents.push_back(head_commit);
        }
    }

    Commit commit;
    commit.tree_id = root_tree;
    commit.parents = parents;
    commit.author = CommitIdentity::now(user_name, user_email);
    commit.committer = commit.author;
    commit.message = message;

    std::vector<uint8_t> serialized = commit.serialize();
    ObjectId commit_id = objects_->write_object(ObjectType::Commit, serialized);

    if (refs_->is_head_detached()) {
        refs_->set_head_detached(commit_id);
    } else {
        std::string branch = refs_->get_current_branch();
        if (branch.empty()) branch = "main";
        refs_->set_branch_commit(branch, commit_id);
    }

    return commit_id;
}

void Repository::create_branch(const std::string& branch_name, const ObjectId& start_point) {
    if (refs_->branch_exists(branch_name)) {
        throw std::runtime_error("A branch named '" + branch_name + "' already exists");
    }

    ObjectId commit_id = start_point ? start_point : refs_->resolve_head();
    if (!commit_id) {
        throw std::runtime_error("Cannot create branch '" + branch_name + "' from empty repository");
    }

    refs_->set_branch_commit(branch_name, commit_id);
}

void Repository::delete_branch(const std::string& branch_name, bool force) {
    (void)force;
    if (refs_->get_current_branch() == branch_name) {
        throw std::runtime_error("Cannot delete currently checked-out branch '" + branch_name + "'");
    }
    if (!refs_->branch_exists(branch_name)) {
        throw std::runtime_error("Branch '" + branch_name + "' not found");
    }
    if (!refs_->delete_branch(branch_name)) {
        throw std::runtime_error("Failed to delete branch '" + branch_name + "'");
    }
}

void Repository::switch_branch(const std::string& branch_name, bool create_new) {
    if (is_bare_) {
        throw std::runtime_error("Cannot switch branches in a bare repository");
    }

    if (create_new) {
        create_branch(branch_name);
        refs_->set_head_symbolic(branch_name);
        return;
    }

    if (!refs_->branch_exists(branch_name)) {
        throw std::runtime_error("Branch '" + branch_name + "' does not exist");
    }

    if (refs_->get_current_branch() == branch_name && !refs_->is_head_detached()) {
        return; // Already on this branch
    }

    // Safety: ensure no uncommitted local changes that would be overwritten
    RepoStatus status = compute_status();
    if (!status.staged_new.empty() || !status.staged_modified.empty() || !status.staged_deleted.empty() ||
        !status.unstaged_modified.empty() || !status.unstaged_deleted.empty()) {
        throw std::runtime_error("Your local changes would be overwritten by switch. Please commit or restore your changes first.");
    }

    ObjectId target_commit_id = *refs_->get_branch_commit(branch_name);
    RawObject target_cobj = objects_->read_object(target_commit_id);
    Commit target_commit = Commit::deserialize(target_cobj.data);
    auto target_files = Tree::flatten_tree(*objects_, target_commit.tree_id);

    ObjectId cur_head_id = refs_->resolve_head();
    std::map<std::string, TreeEntry> cur_files;
    if (cur_head_id) {
        RawObject cur_cobj = objects_->read_object(cur_head_id);
        Commit cur_commit = Commit::deserialize(cur_cobj.data);
        cur_files = Tree::flatten_tree(*objects_, cur_commit.tree_id);
    }

    // Delete files in current branch that don't exist in target branch
    for (const auto& [path, te] : cur_files) {
        if (target_files.find(path) == target_files.end()) {
            std::error_code ec;
            std::filesystem::remove(work_dir_ / path, ec);
        }
    }

    // Write all files from target branch to working tree and rebuild index
    index_->clear();
    for (const auto& [path, te] : target_files) {
        RawObject bobj = objects_->read_object(te.id);
        std::filesystem::path full_p = work_dir_ / path;
        fs_util::write_file_bytes(full_p, bobj.data.data(), bobj.data.size());
        fs_util::set_file_mode(full_p, te.mode);

        IndexEntry ie;
        ie.path = path;
        ie.id = te.id;
        ie.mode = te.mode;
        ie.size = bobj.data.size();
        ie.mtime_ms = fs_util::get_mtime_ms(full_p);
        index_->add_or_update(std::move(ie));
    }

    index_->save_to_file(git_dir_ / "index");
    refs_->set_head_symbolic(branch_name);
}

void Repository::restore_file(const std::string& rel_path, bool staged, const std::optional<ObjectId>& commit_id) {
    if (is_bare_) return;

    if (staged) {
        // Unstage: restore index from HEAD or specified commit
        ObjectId cid = commit_id ? *commit_id : refs_->resolve_head();
        if (!cid) {
            // Unborn branch: unstage simply removes from index
            index_->remove(rel_path);
        } else {
            RawObject cobj = objects_->read_object(cid);
            Commit commit = Commit::deserialize(cobj.data);
            auto tree_files = Tree::flatten_tree(*objects_, commit.tree_id);
            auto it = tree_files.find(rel_path);
            if (it != tree_files.end()) {
                IndexEntry ie;
                ie.path = rel_path;
                ie.id = it->second.id;
                ie.mode = it->second.mode;
                ie.size = 0;
                ie.mtime_ms = 0;
                index_->add_or_update(std::move(ie));
            } else {
                index_->remove(rel_path);
            }
        }
        index_->save_to_file(git_dir_ / "index");
        return;
    }

    // Working directory restore from index (or commit)
    if (commit_id) {
        RawObject cobj = objects_->read_object(*commit_id);
        Commit commit = Commit::deserialize(cobj.data);
        auto tree_files = Tree::flatten_tree(*objects_, commit.tree_id);
        auto it = tree_files.find(rel_path);
        if (it == tree_files.end()) {
            throw std::runtime_error("Path '" + rel_path + "' does not exist in commit " + commit_id->to_short_hex());
        }
        RawObject bobj = objects_->read_object(it->second.id);
        fs_util::write_file_bytes(work_dir_ / rel_path, bobj.data.data(), bobj.data.size());
        fs_util::set_file_mode(work_dir_ / rel_path, it->second.mode);
        return;
    }

    const IndexEntry* ie = index_->find(rel_path);
    if (!ie) {
        throw std::runtime_error("Path '" + rel_path + "' does not exist in index");
    }

    RawObject bobj = objects_->read_object(ie->id);
    fs_util::write_file_bytes(work_dir_ / rel_path, bobj.data.data(), bobj.data.size());
    fs_util::set_file_mode(work_dir_ / rel_path, ie->mode);
}

void Repository::reset(const ObjectId& target_commit, ResetMode mode) {
    if (!target_commit) {
        throw std::runtime_error("Cannot reset: target commit is empty");
    }

    RawObject cobj = objects_->read_object(target_commit);
    if (cobj.type != ObjectType::Commit) {
        throw std::runtime_error("Reset target is not a commit: " + target_commit.to_hex());
    }
    Commit commit = Commit::deserialize(cobj.data);

    // Update ref pointer
    if (refs_->is_head_detached()) {
        refs_->set_head_detached(target_commit);
    } else {
        std::string branch = refs_->get_current_branch();
        refs_->set_branch_commit(branch, target_commit);
    }

    if (mode == ResetMode::Soft) {
        return;
    }

    // Mixed & Hard reset: update index to match commit tree
    auto tree_files = Tree::flatten_tree(*objects_, commit.tree_id);

    if (mode == ResetMode::Hard && !is_bare_) {
        // Remove files that were tracked in index but do not exist in target tree
        for (const auto& [path, ie] : index_->entries()) {
            if (tree_files.find(path) == tree_files.end()) {
                std::error_code ec;
                std::filesystem::remove(work_dir_ / path, ec);
            }
        }
        // Overwrite working directory files with target tree
        for (const auto& [path, te] : tree_files) {
            RawObject bobj = objects_->read_object(te.id);
            std::filesystem::path full_p = work_dir_ / path;
            fs_util::write_file_bytes(full_p, bobj.data.data(), bobj.data.size());
            fs_util::set_file_mode(full_p, te.mode);
        }
    }

    index_->clear();
    for (const auto& [path, te] : tree_files) {
        IndexEntry ie;
        ie.path = path;
        ie.id = te.id;
        ie.mode = te.mode;
        std::filesystem::path full_p = work_dir_ / path;
        if (mode == ResetMode::Hard && std::filesystem::exists(full_p)) {
            ie.size = std::filesystem::file_size(full_p);
            ie.mtime_ms = fs_util::get_mtime_ms(full_p);
        } else {
            ie.size = 0;
            ie.mtime_ms = 0;
        }
        index_->add_or_update(std::move(ie));
    }
    index_->save_to_file(git_dir_ / "index");
}

void Repository::remove_path(const std::string& rel_path, bool cached_only) {
    if (!index_->remove(rel_path)) {
        throw std::runtime_error("Path '" + rel_path + "' is not tracked");
    }
    index_->save_to_file(git_dir_ / "index");

    if (!cached_only && !is_bare_) {
        std::error_code ec;
        std::filesystem::remove(work_dir_ / rel_path, ec);
    }
}

void Repository::move_path(const std::string& src_path, const std::string& dest_path) {
    if (is_bare_) {
        throw std::runtime_error("Cannot move files in a bare repository");
    }

    const IndexEntry* ie = index_->find(src_path);
    if (!ie) {
        throw std::runtime_error("Source '" + src_path + "' is not tracked");
    }

    IndexEntry new_entry = *ie;
    new_entry.path = dest_path;

    std::filesystem::path src_full = work_dir_ / src_path;
    std::filesystem::path dest_full = work_dir_ / dest_path;

    if (dest_full.has_parent_path()) {
        std::filesystem::create_directories(dest_full.parent_path());
    }

    std::error_code ec;
    std::filesystem::rename(src_full, dest_full, ec);
    if (ec) {
        throw std::runtime_error("Rename failed: " + ec.message());
    }

    index_->remove(src_path);
    index_->add_or_update(std::move(new_entry));
    index_->save_to_file(git_dir_ / "index");
}

std::vector<FileDiff> Repository::diff_working_vs_index() {
    std::vector<FileDiff> diffs;
    if (is_bare_) return diffs;

    for (const auto& [path, ie] : index_->entries()) {
        std::filesystem::path full_p = work_dir_ / path;
        if (!std::filesystem::exists(full_p)) {
            // Deleted in working tree
            RawObject bobj = objects_->read_object(ie.id);
            std::string old_content(bobj.data.begin(), bobj.data.end());
            FileDiff fd = DiffEngine::compute_diff(old_content, "", path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        } else {
            std::vector<uint8_t> disk_data = fs_util::read_file_bytes(full_p);
            ObjectId disk_id = hash_bytes(disk_data.data(), disk_data.size());
            if (disk_id != ie.id) {
                RawObject bobj = objects_->read_object(ie.id);
                std::string old_content(bobj.data.begin(), bobj.data.end());
                std::string new_content(disk_data.begin(), disk_data.end());
                FileDiff fd = DiffEngine::compute_diff(old_content, new_content, path, path);
                if (!fd.empty()) diffs.push_back(std::move(fd));
            }
        }
    }
    return diffs;
}

std::vector<FileDiff> Repository::diff_index_vs_head() {
    std::vector<FileDiff> diffs;

    ObjectId head_commit = refs_->resolve_head();
    std::map<std::string, TreeEntry> head_files;
    if (head_commit) {
        RawObject cobj = objects_->read_object(head_commit);
        Commit commit = Commit::deserialize(cobj.data);
        head_files = Tree::flatten_tree(*objects_, commit.tree_id);
    }

    const auto& idx_entries = index_->entries();

    for (const auto& [path, ie] : idx_entries) {
        auto it = head_files.find(path);
        if (it == head_files.end()) {
            // New staged file
            RawObject bobj = objects_->read_object(ie.id);
            std::string new_content(bobj.data.begin(), bobj.data.end());
            FileDiff fd = DiffEngine::compute_diff("", new_content, path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        } else if (it->second.id != ie.id) {
            // Modified staged file
            RawObject old_obj = objects_->read_object(it->second.id);
            RawObject new_obj = objects_->read_object(ie.id);
            std::string old_content(old_obj.data.begin(), old_obj.data.end());
            std::string new_content(new_obj.data.begin(), new_obj.data.end());
            FileDiff fd = DiffEngine::compute_diff(old_content, new_content, path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        }
    }

    for (const auto& [path, te] : head_files) {
        if (idx_entries.find(path) == idx_entries.end()) {
            // Deleted staged file
            RawObject old_obj = objects_->read_object(te.id);
            std::string old_content(old_obj.data.begin(), old_obj.data.end());
            FileDiff fd = DiffEngine::compute_diff(old_content, "", path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        }
    }

    return diffs;
}

std::vector<FileDiff> Repository::diff_commit(const ObjectId& commit_id) {
    std::vector<FileDiff> diffs;
    RawObject cobj = objects_->read_object(commit_id);
    if (cobj.type != ObjectType::Commit) return diffs;

    Commit commit = Commit::deserialize(cobj.data);
    auto cur_files = Tree::flatten_tree(*objects_, commit.tree_id);

    std::map<std::string, TreeEntry> parent_files;
    if (!commit.parents.empty()) {
        RawObject pobj = objects_->read_object(commit.parents.front());
        if (pobj.type == ObjectType::Commit) {
            Commit pcommit = Commit::deserialize(pobj.data);
            parent_files = Tree::flatten_tree(*objects_, pcommit.tree_id);
        }
    }

    for (const auto& [path, te] : cur_files) {
        auto it = parent_files.find(path);
        if (it == parent_files.end()) {
            RawObject bobj = objects_->read_object(te.id);
            std::string new_content(bobj.data.begin(), bobj.data.end());
            FileDiff fd = DiffEngine::compute_diff("", new_content, path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        } else if (it->second.id != te.id) {
            RawObject old_obj = objects_->read_object(it->second.id);
            RawObject new_obj = objects_->read_object(te.id);
            std::string old_content(old_obj.data.begin(), old_obj.data.end());
            std::string new_content(new_obj.data.begin(), new_obj.data.end());
            FileDiff fd = DiffEngine::compute_diff(old_content, new_content, path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        }
    }

    for (const auto& [path, te] : parent_files) {
        if (cur_files.find(path) == cur_files.end()) {
            RawObject old_obj = objects_->read_object(te.id);
            std::string old_content(old_obj.data.begin(), old_obj.data.end());
            FileDiff fd = DiffEngine::compute_diff(old_content, "", path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        }
    }

    return diffs;
}

static void mark_reachable_tree(const ObjectStore& store, const ObjectId& tree_id, std::unordered_set<ObjectId>& marked) {
    if (tree_id.is_zero() || marked.count(tree_id)) return;
    marked.insert(tree_id);

    try {
        RawObject obj = store.read_object(tree_id);
        if (obj.type != ObjectType::Tree) return;
        Tree tree = Tree::deserialize(obj.data);
        for (const auto& entry : tree.entries()) {
            marked.insert(entry.id);
            if (entry.type == ObjectType::Tree) {
                mark_reachable_tree(store, entry.id, marked);
            }
        }
    } catch (...) {}
}

static void mark_reachable_commit(const ObjectStore& store, const ObjectId& commit_id, std::unordered_set<ObjectId>& marked) {
    if (commit_id.is_zero() || marked.count(commit_id)) return;
    marked.insert(commit_id);

    try {
        RawObject obj = store.read_object(commit_id);
        if (obj.type != ObjectType::Commit) return;
        Commit commit = Commit::deserialize(obj.data);
        mark_reachable_tree(store, commit.tree_id, marked);
        for (const auto& p : commit.parents) {
            mark_reachable_commit(store, p, marked);
        }
    } catch (...) {}
}

Repository::GcResult Repository::garbage_collect() {
    GcResult res;
    std::vector<ObjectId> all_objects = objects_->list_all_objects();
    res.total_objects = all_objects.size();

    std::unordered_set<ObjectId> reachable;

    // 1. Mark from all refs
    auto all_refs = refs_->list_all_references();
    for (const auto& [rname, rid] : all_refs) {
        mark_reachable_commit(*objects_, rid, reachable);
    }

    // 2. Mark from index
    for (const auto& [path, ie] : index_->entries()) {
        reachable.insert(ie.id);
    }

    res.kept_objects = 0;
    res.removed_objects = 0;

    // 3. Sweep unreachable objects
    for (const auto& id : all_objects) {
        if (reachable.find(id) != reachable.end()) {
            res.kept_objects++;
        } else {
            if (objects_->remove_object(id)) {
                res.removed_objects++;
            }
        }
    }

    return res;
}

std::vector<Repository::FsckIssue> Repository::verify_integrity() {
    std::vector<FsckIssue> issues;

    // Check all objects in store
    std::vector<ObjectId> all_objects = objects_->list_all_objects();
    for (const auto& id : all_objects) {
        try {
            RawObject obj = objects_->read_object(id);
            if (obj.type == ObjectType::Commit) {
                Commit commit = Commit::deserialize(obj.data);
                if (!objects_->has_object(commit.tree_id)) {
                    issues.push_back({FsckIssue::Severity::Error, "Commit " + id.to_short_hex() + " references missing root tree " + commit.tree_id.to_short_hex()});
                }
                for (const auto& p : commit.parents) {
                    if (!objects_->has_object(p)) {
                        issues.push_back({FsckIssue::Severity::Error, "Commit " + id.to_short_hex() + " references missing parent " + p.to_short_hex()});
                    }
                }
            } else if (obj.type == ObjectType::Tree) {
                Tree tree = Tree::deserialize(obj.data);
                for (const auto& entry : tree.entries()) {
                    if (!objects_->has_object(entry.id)) {
                        issues.push_back({FsckIssue::Severity::Error, "Tree " + id.to_short_hex() + " references missing " + std::string(to_string(entry.type)) + " " + entry.id.to_short_hex() + " (" + entry.name + ")"});
                    }
                }
            }
        } catch (const std::exception& e) {
            issues.push_back({FsckIssue::Severity::Error, "Object " + id.to_short_hex() + " verification failed: " + e.what()});
        }
    }

    // Check all refs
    auto all_refs = refs_->list_all_references();
    for (const auto& [rname, rid] : all_refs) {
        if (!objects_->has_object(rid)) {
            issues.push_back({FsckIssue::Severity::Error, "Reference '" + rname + "' points to missing commit " + rid.to_short_hex()});
        }
    }

    return issues;
}

std::optional<ObjectId> Repository::resolve_revision(const std::string& rev) const {
    if (rev.empty()) return std::nullopt;
    if (rev == "HEAD") {
        ObjectId head_id = refs_->resolve_head();
        if (head_id) return head_id;
        return std::nullopt;
    }
    if (refs_->branch_exists(rev)) {
        return refs_->get_branch_commit(rev);
    }
    if (auto tag_id = refs_->get_tag(rev)) {
        return tag_id;
    }
    if (rev.size() == 64) {
        try {
            ObjectId id = ObjectId::from_hex(rev);
            if (objects_->has_object(id)) return id;
        } catch (...) {}
    }
    if (rev.size() >= 4 && rev.size() < 64) {
        return objects_->resolve_prefix(rev);
    }
    return std::nullopt;
}

void Repository::create_tag(const std::string& name, const ObjectId& target, const std::string& message) {
    (void)message;
    if (name.empty()) {
        throw std::runtime_error("Tag name cannot be empty");
    }
    if (refs_->get_tag(name)) {
        throw std::runtime_error("Tag '" + name + "' already exists");
    }
    ObjectId target_id = target ? target : refs_->resolve_head();
    if (!target_id) {
        throw std::runtime_error("Cannot create tag '" + name + "': repository has no commits");
    }
    refs_->set_tag(name, target_id);
}

void Repository::delete_tag(const std::string& name) {
    if (!refs_->get_tag(name)) {
        throw std::runtime_error("Tag '" + name + "' not found");
    }
    if (!refs_->delete_tag(name)) {
        throw std::runtime_error("Failed to delete tag '" + name + "'");
    }
}

std::vector<std::string> Repository::list_tags() const {
    return refs_->list_tags();
}

std::vector<Repository::StashEntry> Repository::list_stashes() const {
    std::vector<StashEntry> result;
    std::filesystem::path stack_file = git_dir_ / "stash_stack";
    if (!std::filesystem::exists(stack_file)) {
        return result;
    }
    std::ifstream ifs(stack_file);
    std::string line;
    size_t idx = 0;
    while (std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t p1 = line.find('|');
        size_t p2 = (p1 != std::string::npos) ? line.find('|', p1 + 1) : std::string::npos;
        size_t p3 = (p2 != std::string::npos) ? line.find('|', p2 + 1) : std::string::npos;
        if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
            try {
                StashEntry se;
                se.index = idx++;
                se.commit_id = ObjectId::from_hex(line.substr(0, p1));
                se.timestamp = std::stoll(line.substr(p1 + 1, p2 - p1 - 1));
                se.branch = line.substr(p2 + 1, p3 - p2 - 1);
                se.message = line.substr(p3 + 1);
                result.push_back(std::move(se));
            } catch (...) {}
        }
    }
    return result;
}

ObjectId Repository::stash_push(const std::string& message) {
    RepoStatus status = compute_status();
    if (status.is_clean()) {
        throw std::runtime_error("No local changes to save");
    }
    ObjectId head_id = refs_->resolve_head();
    if (!head_id) {
        throw std::runtime_error("Cannot stash on an empty repository");
    }

    std::string branch = refs_->get_current_branch();
    if (branch.empty()) branch = "detached";

    stage_all();
    ObjectId stash_tree = write_tree_from_index();

    std::string user_name = config_->get_or("user.name", "User");
    std::string user_email = config_->get_or("user.email", "user@localhost");

    Commit sc;
    sc.tree_id = stash_tree;
    sc.parents = { head_id };
    sc.author = CommitIdentity::now(user_name, user_email);
    sc.committer = sc.author;
    std::string full_msg = message.empty() ? ("WIP on " + branch + ": " + head_id.to_short_hex()) : message;
    sc.message = full_msg;

    ObjectId stash_commit_id = objects_->write_object(ObjectType::Commit, sc.serialize());

    std::filesystem::path stack_file = git_dir_ / "stash_stack";
    std::string existing;
    if (std::filesystem::exists(stack_file)) {
        existing = fs_util::read_file_string(stack_file);
    }
    std::string new_entry = stash_commit_id.to_hex() + "|" + std::to_string(sc.author.timestamp) + "|" + branch + "|" + full_msg + "\n";
    fs_util::atomic_write_file(stack_file, new_entry + existing);

    reset(head_id, ResetMode::Hard);
    return stash_commit_id;
}

void Repository::stash_apply(size_t index) {
    auto stashes = list_stashes();
    if (index >= stashes.size()) {
        throw std::runtime_error("stash@{" + std::to_string(index) + "} is not a valid stash reference");
    }
    const auto& entry = stashes[index];
    RawObject cobj = objects_->read_object(entry.commit_id);
    Commit sc = Commit::deserialize(cobj.data);
    auto stash_files = Tree::flatten_tree(*objects_, sc.tree_id);

    for (const auto& [path, te] : stash_files) {
        RawObject bobj = objects_->read_object(te.id);
        std::filesystem::path full_p = work_dir_ / path;
        fs_util::write_file_bytes(full_p, bobj.data.data(), bobj.data.size());
        fs_util::set_file_mode(full_p, te.mode);

        IndexEntry ie;
        ie.path = path;
        ie.id = te.id;
        ie.mode = te.mode;
        ie.size = bobj.data.size();
        ie.mtime_ms = fs_util::get_mtime_ms(full_p);
        index_->add_or_update(std::move(ie));
    }
    index_->save_to_file(git_dir_ / "index");
}

void Repository::stash_drop(size_t index) {
    auto stashes = list_stashes();
    if (index >= stashes.size()) {
        throw std::runtime_error("stash@{" + std::to_string(index) + "} is not a valid stash reference");
    }
    stashes.erase(stashes.begin() + index);

    std::filesystem::path stack_file = git_dir_ / "stash_stack";
    if (stashes.empty()) {
        std::error_code ec;
        std::filesystem::remove(stack_file, ec);
    } else {
        std::string content;
        for (const auto& s : stashes) {
            content += s.commit_id.to_hex() + "|" + std::to_string(s.timestamp) + "|" + s.branch + "|" + s.message + "\n";
        }
        fs_util::atomic_write_file(stack_file, content);
    }
}

void Repository::stash_pop(size_t index) {
    stash_apply(index);
    stash_drop(index);
}

void Repository::stash_clear() {
    std::filesystem::path stack_file = git_dir_ / "stash_stack";
    std::error_code ec;
    std::filesystem::remove(stack_file, ec);
}

static std::pair<std::string, bool> merge_text_content(
    const std::string& base_text,
    const std::string& head_text,
    const std::string& target_text,
    const std::string& target_label) {
    if (head_text == base_text) {
        return {target_text, false};
    }
    if (target_text == base_text) {
        return {head_text, false};
    }
    if (head_text == target_text) {
        return {head_text, false};
    }

    std::ostringstream ss;
    ss << "<<<<<<< HEAD\n";
    ss << head_text;
    if (!head_text.empty() && head_text.back() != '\n') ss << "\n";
    ss << "=======\n";
    ss << target_text;
    if (!target_text.empty() && target_text.back() != '\n') ss << "\n";
    ss << ">>>>>>> " << target_label << "\n";

    return {ss.str(), true};
}

Repository::MergeResult Repository::merge(const std::string& branch_or_commit) {
    auto target_opt = resolve_revision(branch_or_commit);
    if (!target_opt) {
        throw std::runtime_error("Branch or revision '" + branch_or_commit + "' not found");
    }
    ObjectId target_id = *target_opt;
    ObjectId head_id = refs_->resolve_head();
    if (!head_id) {
        throw std::runtime_error("Cannot merge: HEAD points to unborn branch");
    }

    if (target_id == head_id) {
        MergeResult r;
        r.already_up_to_date = true;
        r.commit_id = head_id;
        r.message = "Already up to date.";
        return r;
    }

    std::unordered_set<ObjectId> head_ancestors;
    std::queue<ObjectId> q1;
    q1.push(head_id);
    head_ancestors.insert(head_id);
    while (!q1.empty()) {
        ObjectId c = q1.front();
        q1.pop();
        if (!objects_->has_object(c)) continue;
        RawObject ro = objects_->read_object(c);
        if (ro.type != ObjectType::Commit) continue;
        Commit com = Commit::deserialize(ro.data);
        for (const auto& p : com.parents) {
            if (head_ancestors.insert(p).second) {
                q1.push(p);
            }
        }
    }

    ObjectId merge_base = ObjectId::zero();
    std::queue<ObjectId> q2;
    std::unordered_set<ObjectId> visited2;
    q2.push(target_id);
    visited2.insert(target_id);
    while (!q2.empty()) {
        ObjectId c = q2.front();
        q2.pop();
        if (head_ancestors.count(c)) {
            merge_base = c;
            break;
        }
        if (!objects_->has_object(c)) continue;
        RawObject ro = objects_->read_object(c);
        if (ro.type != ObjectType::Commit) continue;
        Commit com = Commit::deserialize(ro.data);
        for (const auto& p : com.parents) {
            if (visited2.insert(p).second) {
                q2.push(p);
            }
        }
    }

    if (merge_base == target_id) {
        MergeResult r;
        r.already_up_to_date = true;
        r.commit_id = head_id;
        r.message = "Already up to date.";
        return r;
    }

    if (merge_base == head_id) {
        reset(target_id, ResetMode::Hard);
        MergeResult r;
        r.is_fast_forward = true;
        r.commit_id = target_id;
        r.message = "Fast-forward";
        return r;
    }

    RepoStatus st = compute_status();
    if (!st.staged_new.empty() || !st.staged_modified.empty() || !st.staged_deleted.empty() ||
        !st.unstaged_modified.empty() || !st.unstaged_deleted.empty()) {
        throw std::runtime_error("Your local changes would be overwritten by merge. Please commit or stash your changes first.");
    }

    std::map<std::string, TreeEntry> base_files;
    if (merge_base) {
        Commit bc = Commit::deserialize(objects_->read_object(merge_base).data);
        base_files = Tree::flatten_tree(*objects_, bc.tree_id);
    }
    Commit hc = Commit::deserialize(objects_->read_object(head_id).data);
    auto head_files = Tree::flatten_tree(*objects_, hc.tree_id);
    Commit tc = Commit::deserialize(objects_->read_object(target_id).data);
    auto target_files = Tree::flatten_tree(*objects_, tc.tree_id);

    std::set<std::string> all_paths;
    for (const auto& [p, _] : base_files) all_paths.insert(p);
    for (const auto& [p, _] : head_files) all_paths.insert(p);
    for (const auto& [p, _] : target_files) all_paths.insert(p);

    MergeResult result;
    for (const auto& path : all_paths) {
        auto it_b = base_files.find(path);
        auto it_h = head_files.find(path);
        auto it_t = target_files.find(path);

        bool in_b = (it_b != base_files.end());
        bool in_h = (it_h != head_files.end());
        bool in_t = (it_t != target_files.end());

        if (in_h && in_t) {
            if (it_h->second.id == it_t->second.id) {
                continue;
            }
            if (in_b && it_h->second.id == it_b->second.id) {
                RawObject to = objects_->read_object(it_t->second.id);
                std::filesystem::path fp = work_dir_ / path;
                fs_util::write_file_bytes(fp, to.data.data(), to.data.size());
                fs_util::set_file_mode(fp, it_t->second.mode);
                IndexEntry ie;
                ie.path = path;
                ie.id = it_t->second.id;
                ie.mode = it_t->second.mode;
                ie.size = to.data.size();
                ie.mtime_ms = fs_util::get_mtime_ms(fp);
                index_->add_or_update(std::move(ie));
                result.merged_files.push_back(path);
            } else if (in_b && it_t->second.id == it_b->second.id) {
                continue;
            } else {
                std::string b_txt = in_b ? fs_util::read_file_string(work_dir_ / path) : "";
                if (in_b) {
                    RawObject bo = objects_->read_object(it_b->second.id);
                    b_txt = std::string(bo.data.begin(), bo.data.end());
                }
                RawObject ho = objects_->read_object(it_h->second.id);
                std::string h_txt(ho.data.begin(), ho.data.end());
                RawObject to = objects_->read_object(it_t->second.id);
                std::string t_txt(to.data.begin(), to.data.end());

                auto [m_txt, conflict] = merge_text_content(b_txt, h_txt, t_txt, branch_or_commit);
                std::filesystem::path fp = work_dir_ / path;
                fs_util::atomic_write_file(fp, m_txt);
                ObjectId new_blob = objects_->write_object(ObjectType::Blob, std::vector<uint8_t>(m_txt.begin(), m_txt.end()));

                IndexEntry ie;
                ie.path = path;
                ie.id = new_blob;
                ie.mode = it_h->second.mode;
                ie.size = m_txt.size();
                ie.mtime_ms = fs_util::get_mtime_ms(fp);
                index_->add_or_update(std::move(ie));

                if (conflict) {
                    result.has_conflicts = true;
                    result.conflict_files.push_back(path);
                } else {
                    result.merged_files.push_back(path);
                }
            }
        } else if (in_t && !in_h) {
            if (in_b && it_t->second.id == it_b->second.id) {
                // Deleted in HEAD, unmodified in target
            } else if (!in_b) {
                RawObject to = objects_->read_object(it_t->second.id);
                std::filesystem::path fp = work_dir_ / path;
                fs_util::write_file_bytes(fp, to.data.data(), to.data.size());
                fs_util::set_file_mode(fp, it_t->second.mode);
                IndexEntry ie;
                ie.path = path;
                ie.id = it_t->second.id;
                ie.mode = it_t->second.mode;
                ie.size = to.data.size();
                ie.mtime_ms = fs_util::get_mtime_ms(fp);
                index_->add_or_update(std::move(ie));
                result.merged_files.push_back(path);
            } else {
                result.has_conflicts = true;
                result.conflict_files.push_back(path);
            }
        } else if (in_h && !in_t) {
            if (in_b && it_h->second.id == it_b->second.id) {
                std::error_code ec;
                std::filesystem::remove(work_dir_ / path, ec);
                index_->remove(path);
                result.merged_files.push_back(path);
            } else if (!in_b) {
                // Kept in HEAD
            } else {
                result.has_conflicts = true;
                result.conflict_files.push_back(path);
            }
        }
    }

    index_->save_to_file(git_dir_ / "index");

    if (result.has_conflicts) {
        fs_util::atomic_write_file(git_dir_ / "MERGE_HEAD", target_id.to_hex() + "\n");
        result.message = "Automatic merge failed; fix conflicts and then commit the result.";
        return result;
    }

    std::string user_name = config_->get_or("user.name", "User");
    std::string user_email = config_->get_or("user.email", "user@localhost");

    Commit mc;
    mc.tree_id = write_tree_from_index();
    mc.parents = { head_id, target_id };
    mc.author = CommitIdentity::now(user_name, user_email);
    mc.committer = mc.author;
    mc.message = "Merge branch '" + branch_or_commit + "'";

    ObjectId merge_cid = objects_->write_object(ObjectType::Commit, mc.serialize());
    if (refs_->is_head_detached()) {
        refs_->set_head_detached(merge_cid);
    } else {
        refs_->set_branch_commit(refs_->get_current_branch(), merge_cid);
    }

    std::error_code ec;
    std::filesystem::remove(git_dir_ / "MERGE_HEAD", ec);

    result.commit_id = merge_cid;
    result.message = "Merge made by the 'three-way' strategy.";
    return result;
}

ObjectId Repository::cherry_pick(const ObjectId& commit_id) {
    if (!objects_->has_object(commit_id)) {
        throw std::runtime_error("Commit " + commit_id.to_short_hex() + " not found");
    }
    RawObject cobj = objects_->read_object(commit_id);
    if (cobj.type != ObjectType::Commit) {
        throw std::runtime_error("Object " + commit_id.to_short_hex() + " is not a commit");
    }
    Commit target_c = Commit::deserialize(cobj.data);

    ObjectId head_id = refs_->resolve_head();
    if (!head_id) {
        throw std::runtime_error("Cannot cherry-pick on an empty repository");
    }

    auto target_files = Tree::flatten_tree(*objects_, target_c.tree_id);
    std::map<std::string, TreeEntry> parent_files;
    if (!target_c.parents.empty()) {
        Commit pc = Commit::deserialize(objects_->read_object(target_c.parents[0]).data);
        parent_files = Tree::flatten_tree(*objects_, pc.tree_id);
    }

    for (const auto& [path, te] : target_files) {
        auto it = parent_files.find(path);
        if (it == parent_files.end() || it->second.id != te.id) {
            RawObject bobj = objects_->read_object(te.id);
            std::filesystem::path fp = work_dir_ / path;
            fs_util::write_file_bytes(fp, bobj.data.data(), bobj.data.size());
            fs_util::set_file_mode(fp, te.mode);

            IndexEntry ie;
            ie.path = path;
            ie.id = te.id;
            ie.mode = te.mode;
            ie.size = bobj.data.size();
            ie.mtime_ms = fs_util::get_mtime_ms(fp);
            index_->add_or_update(std::move(ie));
        }
    }
    for (const auto& [path, te] : parent_files) {
        if (target_files.find(path) == target_files.end()) {
            std::error_code ec;
            std::filesystem::remove(work_dir_ / path, ec);
            index_->remove(path);
        }
    }

    index_->save_to_file(git_dir_ / "index");

    std::string user_name = config_->get_or("user.name", "User");
    std::string user_email = config_->get_or("user.email", "user@localhost");

    Commit new_c;
    new_c.tree_id = write_tree_from_index();
    new_c.parents = { head_id };
    new_c.author = target_c.author;
    new_c.committer = CommitIdentity::now(user_name, user_email);
    new_c.message = target_c.message;

    ObjectId new_id = objects_->write_object(ObjectType::Commit, new_c.serialize());
    if (refs_->is_head_detached()) {
        refs_->set_head_detached(new_id);
    } else {
        refs_->set_branch_commit(refs_->get_current_branch(), new_id);
    }

    return new_id;
}

std::string Repository::export_git_fastimport() const {
    std::ostringstream ss;

    std::vector<std::string> branches = refs_->list_branches();
    std::vector<ObjectId> tip_commits;
    for (const auto& b : branches) {
        if (auto cid = refs_->get_branch_commit(b)) {
            tip_commits.push_back(*cid);
        }
    }
    if (refs_->is_head_detached()) {
        if (auto hid = refs_->resolve_head()) {
            tip_commits.push_back(hid);
        }
    }

    std::unordered_set<ObjectId> visited;
    std::vector<ObjectId> topo_order;
    std::function<void(const ObjectId&)> dfs = [&](const ObjectId& id) {
        if (!visited.insert(id).second) return;
        if (!objects_->has_object(id)) return;
        RawObject obj = objects_->read_object(id);
        if (obj.type != ObjectType::Commit) return;
        Commit c = Commit::deserialize(obj.data);
        for (const auto& p : c.parents) {
            dfs(p);
        }
        topo_order.push_back(id);
    };

    for (const auto& tip : tip_commits) {
        dfs(tip);
    }

    std::unordered_map<ObjectId, size_t> mark_map;
    size_t mark = 1;
    for (const auto& cid : topo_order) {
        mark_map[cid] = mark++;
    }

    std::string cur_branch = refs_->get_current_branch();
    if (cur_branch.empty()) cur_branch = "main";

    for (const auto& cid : topo_order) {
        Commit c = Commit::deserialize(objects_->read_object(cid).data);
        size_t my_mark = mark_map[cid];

        ss << "commit refs/heads/" << cur_branch << "\n";
        ss << "mark :" << my_mark << "\n";
        ss << "author " << c.author.format() << "\n";
        ss << "committer " << c.committer.format() << "\n";
        ss << "data " << c.message.size() << "\n";
        ss << c.message << "\n";

        if (!c.parents.empty()) {
            auto it = mark_map.find(c.parents[0]);
            if (it != mark_map.end()) {
                ss << "from :" << it->second << "\n";
            }
            for (size_t i = 1; i < c.parents.size(); ++i) {
                auto it2 = mark_map.find(c.parents[i]);
                if (it2 != mark_map.end()) {
                    ss << "merge :" << it2->second << "\n";
                }
            }
        }

        auto files = Tree::flatten_tree(*objects_, c.tree_id);
        for (const auto& [path, te] : files) {
            RawObject bobj = objects_->read_object(te.id);
            ss << "M " << std::oct << te.mode << std::dec << " inline " << path << "\n";
            ss << "data " << bobj.data.size() << "\n";
            ss.write(reinterpret_cast<const char*>(bobj.data.data()), bobj.data.size());
            ss << "\n";
        }
    }

    for (const auto& tag_name : refs_->list_tags()) {
        if (auto tid = refs_->get_tag(tag_name)) {
            auto it = mark_map.find(*tid);
            if (it != mark_map.end()) {
                ss << "tag " << tag_name << "\n";
                ss << "from :" << it->second << "\n";
                ss << "data 0\n";
            }
        }
    }

    return ss.str();
}

void Repository::export_to_git(const std::filesystem::path& target_git_dir) {
    std::filesystem::create_directories(target_git_dir);
    std::filesystem::path fi_path = target_git_dir / "light-git-export.fi";
    std::string fi_content = export_git_fastimport();
    fs_util::atomic_write_file(fi_path, fi_content);

    std::string check_cmd =
#if defined(_WIN32)
        "git --version > nul 2>&1";
#else
        "git --version > /dev/null 2>&1";
#endif

    if (std::system(check_cmd.c_str()) == 0) {
        std::string init_cmd = "git -C \"" + target_git_dir.string() + "\" init";
        std::system(init_cmd.c_str());

        std::string import_cmd = "git -C \"" + target_git_dir.string() + "\" fast-import < \"" + fi_path.string() + "\"";
        std::system(import_cmd.c_str());

        std::string checkout_cmd = "git -C \"" + target_git_dir.string() + "\" checkout -f main";
        std::system(checkout_cmd.c_str());
    }
}

} // namespace light_git
