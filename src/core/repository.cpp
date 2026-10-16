#include "light_git/repository.hpp"
#include "light_git/filesystem.hpp"
#include "light_git/crypto.hpp"
#include <fstream>
#include <stdexcept>
#include <unordered_set>
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

} // namespace light_git
