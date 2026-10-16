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
#include <cstring>

namespace light_git {

Repository::Repository(std::filesystem::path work_dir, std::filesystem::path git_dir, bool bare)
    : work_dir_(std::move(work_dir)), git_dir_(std::move(git_dir)), is_bare_(bare) {
    std::filesystem::path common_dir = git_dir_;
    std::filesystem::path cdir_file = git_dir_ / "commondir";
    std::filesystem::path head_file = git_dir_ / "HEAD";

    if (std::filesystem::exists(cdir_file)) {
        std::string cdir_str = fs_util::read_file_string(cdir_file);
        while (!cdir_str.empty() && (cdir_str.front() == ' ' || cdir_str.front() == '\t')) cdir_str.erase(0, 1);
        while (!cdir_str.empty() && (cdir_str.back() == '\r' || cdir_str.back() == '\n' || cdir_str.back() == ' ')) cdir_str.pop_back();
        std::filesystem::path p(cdir_str);
        if (p.is_relative()) {
            common_dir = std::filesystem::weakly_canonical(git_dir_ / p);
        } else {
            common_dir = p;
        }
    }

    objects_ = std::make_unique<ObjectStore>(common_dir / "objects");
    refs_ = std::make_unique<References>(common_dir, head_file);
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
    std::filesystem::path lgit_entry = root / ".light-git";
    bool bare = !std::filesystem::exists(lgit_entry);
    std::filesystem::path work_dir = root;
    std::filesystem::path git_dir = bare ? root : lgit_entry;

    if (!bare && std::filesystem::is_regular_file(lgit_entry)) {
        // Linked worktree file: "gitdir: <path>"
        std::string content = fs_util::read_file_string(lgit_entry);
        static constexpr std::string_view prefix = "gitdir:";
        size_t p = content.find(prefix);
        if (p != std::string::npos) {
            std::string path_str = content.substr(p + prefix.size());
            while (!path_str.empty() && (path_str.front() == ' ' || path_str.front() == '\t')) path_str.erase(0, 1);
            while (!path_str.empty() && (path_str.back() == '\r' || path_str.back() == '\n' || path_str.back() == ' ')) path_str.pop_back();
            std::filesystem::path gpath(path_str);
            if (gpath.is_relative()) {
                git_dir = std::filesystem::weakly_canonical(root / gpath);
            } else {
                git_dir = gpath;
            }
        }
    }

    Repository repo(work_dir, git_dir, bare);
    std::filesystem::path cfg_dir = git_dir;
    if (std::filesystem::exists(git_dir / "commondir")) {
        std::string cdir_str = fs_util::read_file_string(git_dir / "commondir");
        while (!cdir_str.empty() && (cdir_str.front() == ' ' || cdir_str.front() == '\t')) cdir_str.erase(0, 1);
        while (!cdir_str.empty() && (cdir_str.back() == '\r' || cdir_str.back() == '\n' || cdir_str.back() == ' ')) cdir_str.pop_back();
        std::filesystem::path cp(cdir_str);
        if (cp.is_relative()) {
            cfg_dir = std::filesystem::weakly_canonical(git_dir / cp);
        } else {
            cfg_dir = cp;
        }
    }
    repo.config_->load(cfg_dir / "config");
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
            if (cobj.type != ObjectType::Commit) {
                throw std::runtime_error("HEAD object " + status.head_commit.to_short_hex() + " is not a commit");
            }
            Commit commit = Commit::deserialize(cobj.data);
            head_files = Tree::flatten_tree(*objects_, commit.tree_id);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to read HEAD commit " + status.head_commit.to_short_hex() + ": " + e.what());
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

std::vector<FileDiff> Repository::diff_commits(const ObjectId& old_commit_id, const ObjectId& new_commit_id) {
    std::vector<FileDiff> diffs;
    std::map<std::string, TreeEntry> old_files;
    std::map<std::string, TreeEntry> new_files;

    if (old_commit_id) {
        RawObject oobj = objects_->read_object(old_commit_id);
        if (oobj.type == ObjectType::Commit) {
            Commit ocommit = Commit::deserialize(oobj.data);
            old_files = Tree::flatten_tree(*objects_, ocommit.tree_id);
        }
    }

    if (new_commit_id) {
        RawObject nobj = objects_->read_object(new_commit_id);
        if (nobj.type == ObjectType::Commit) {
            Commit ncommit = Commit::deserialize(nobj.data);
            new_files = Tree::flatten_tree(*objects_, ncommit.tree_id);
        }
    }

    for (const auto& [path, te] : new_files) {
        auto it = old_files.find(path);
        if (it == old_files.end()) {
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

    for (const auto& [path, te] : old_files) {
        if (new_files.find(path) == new_files.end()) {
            RawObject old_obj = objects_->read_object(te.id);
            std::string old_content(old_obj.data.begin(), old_obj.data.end());
            FileDiff fd = DiffEngine::compute_diff(old_content, "", path, path);
            if (!fd.empty()) diffs.push_back(std::move(fd));
        }
    }

    return diffs;
}

std::vector<FileDiff> Repository::diff_commit(const ObjectId& commit_id) {
    RawObject cobj = objects_->read_object(commit_id);
    if (cobj.type != ObjectType::Commit) return {};
    Commit commit = Commit::deserialize(cobj.data);
    ObjectId parent_id = commit.parents.empty() ? ObjectId::zero() : commit.parents.front();
    return diff_commits(parent_id, commit_id);
}

std::vector<FileDiff> Repository::diff_working_vs_commit(const ObjectId& commit_id) {
    std::vector<FileDiff> diffs;
    if (is_bare_) return diffs;

    std::map<std::string, TreeEntry> commit_files;
    if (commit_id) {
        RawObject cobj = objects_->read_object(commit_id);
        if (cobj.type == ObjectType::Commit) {
            Commit commit = Commit::deserialize(cobj.data);
            commit_files = Tree::flatten_tree(*objects_, commit.tree_id);
        }
    }

    std::unordered_set<std::string> on_disk_paths;
    auto entries = fs_util::list_directory_recursive(work_dir_);
    for (const auto& de : entries) {
        if (de.is_directory) continue;
        std::string rel_path = fs_util::normalize_path(de.relative_path);
        on_disk_paths.insert(rel_path);

        auto it = commit_files.find(rel_path);
        if (it == commit_files.end()) {
            if (!ignores_.is_ignored(rel_path, false)) {
                std::vector<uint8_t> disk_data = fs_util::read_file_bytes(de.full_path);
                std::string new_content(disk_data.begin(), disk_data.end());
                FileDiff fd = DiffEngine::compute_diff("", new_content, rel_path, rel_path);
                if (!fd.empty()) diffs.push_back(std::move(fd));
            }
        } else {
            std::vector<uint8_t> disk_data = fs_util::read_file_bytes(de.full_path);
            ObjectId disk_id = hash_bytes(disk_data.data(), disk_data.size());
            if (disk_id != it->second.id) {
                RawObject old_obj = objects_->read_object(it->second.id);
                std::string old_content(old_obj.data.begin(), old_obj.data.end());
                std::string new_content(disk_data.begin(), disk_data.end());
                FileDiff fd = DiffEngine::compute_diff(old_content, new_content, rel_path, rel_path);
                if (!fd.empty()) diffs.push_back(std::move(fd));
            }
        }
    }

    for (const auto& [path, te] : commit_files) {
        if (on_disk_paths.find(path) == on_disk_paths.end()) {
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
        if (obj.type != ObjectType::Tree) {
            throw std::runtime_error("Object " + tree_id.to_short_hex() + " is not a tree");
        }
        Tree tree = Tree::deserialize(obj.data);
        for (const auto& entry : tree.entries()) {
            marked.insert(entry.id);
            if (entry.type == ObjectType::Tree) {
                mark_reachable_tree(store, entry.id, marked);
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Garbage collection aborted: tree " + tree_id.to_short_hex() +
                                 " is corrupted or unreadable: " + e.what() +
                                 ". Run 'lgit fsck' to verify repository integrity.");
    }
}

static void mark_reachable_commit(const ObjectStore& store, const ObjectId& commit_id, std::unordered_set<ObjectId>& marked) {
    if (commit_id.is_zero() || marked.count(commit_id)) return;
    marked.insert(commit_id);

    try {
        RawObject obj = store.read_object(commit_id);
        if (obj.type != ObjectType::Commit) {
            throw std::runtime_error("Object " + commit_id.to_short_hex() + " is not a commit");
        }
        Commit commit = Commit::deserialize(obj.data);
        mark_reachable_tree(store, commit.tree_id, marked);
        for (const auto& p : commit.parents) {
            mark_reachable_commit(store, p, marked);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Garbage collection aborted: commit " + commit_id.to_short_hex() +
                                 " is corrupted or unreadable: " + e.what() +
                                 ". Run 'lgit fsck' to verify repository integrity.");
    }
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

    // 3. Mark from stashes
    for (const auto& s : list_stashes()) {
        mark_reachable_commit(*objects_, s.commit_id, reachable);
    }

    res.kept_objects = 0;
    res.removed_objects = 0;

    // 4. Sweep unreachable objects
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

    // Handle rev~N or rev^N syntax
    size_t tilde_pos = rev.find_first_of("~^");
    if (tilde_pos != std::string::npos) {
        std::string base_str = rev.substr(0, tilde_pos);
        if (base_str.empty()) base_str = "HEAD";
        std::string count_str = rev.substr(tilde_pos + 1);
        unsigned long steps = 1;
        if (!count_str.empty()) {
            try {
                size_t parsed_len = 0;
                steps = std::stoul(count_str, &parsed_len);
                if (parsed_len != count_str.size()) {
                    return std::nullopt;
                }
            } catch (...) {
                return std::nullopt;
            }
        }
        auto base_id = resolve_revision(base_str);
        if (!base_id) return std::nullopt;
        ObjectId cur = *base_id;
        for (unsigned long i = 0; i < steps && cur; ++i) {
            try {
                RawObject robj = objects_->read_object(cur);
                if (robj.type != ObjectType::Commit) return std::nullopt;
                Commit c = Commit::deserialize(robj.data);
                if (c.parents.empty()) return std::nullopt;
                cur = c.parents.front();
            } catch (...) {
                return std::nullopt;
            }
        }
        return cur;
    }

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

    // Check remote tracking branch: "origin/main" or "refs/remotes/origin/main"
    std::string_view rview = rev;
    if (rview.rfind("refs/remotes/", 0) == 0) {
        rview.remove_prefix(13);
    }
    size_t slash = rview.find('/');
    if (slash != std::string_view::npos) {
        std::string remote_part(rview.substr(0, slash));
        std::string branch_part(rview.substr(slash + 1));
        if (auto r_id = refs_->get_remote_branch_commit(remote_part, branch_part)) {
            return r_id;
        }
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
                se.commit_id = ObjectId::from_hex(line.substr(0, p1));
                se.timestamp = std::stoll(line.substr(p1 + 1, p2 - p1 - 1));
                se.branch = line.substr(p2 + 1, p3 - p2 - 1);
                se.message = line.substr(p3 + 1);
                se.index = idx++;
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
        int r1 = std::system(init_cmd.c_str());
        (void)r1;

        std::string import_cmd = "git -C \"" + target_git_dir.string() + "\" fast-import < \"" + fi_path.string() + "\"";
        int r2 = std::system(import_cmd.c_str());
        (void)r2;

        std::string checkout_cmd = "git -C \"" + target_git_dir.string() + "\" checkout -f main";
        int r3 = std::system(checkout_cmd.c_str());
        (void)r3;
    }
}

std::vector<Repository::RemoteInfo> Repository::list_remotes() const {
    std::vector<RemoteInfo> result;
    auto all = config_->get_all_merged();
    std::set<std::string> names;
    for (const auto& [k, v] : all) {
        if (k.rfind("remote.", 0) == 0 && k.size() > 11 && k.substr(k.size() - 4) == ".url") {
            std::string name = k.substr(7, k.size() - 11);
            if (!name.empty() && names.insert(name).second) {
                result.push_back({name, v});
            }
        }
    }
    return result;
}

void Repository::add_remote(const std::string& name, const std::string& url) {
    if (name.empty()) throw std::runtime_error("Remote name cannot be empty");
    if (url.empty()) throw std::runtime_error("Remote URL/path cannot be empty");
    if (get_remote_url(name)) {
        throw std::runtime_error("Remote '" + name + "' already exists");
    }
    config_->set_local("remote." + name + ".url", url);
    config_->save();
}

void Repository::remove_remote(const std::string& name) {
    if (!get_remote_url(name)) {
        throw std::runtime_error("No such remote: '" + name + "'");
    }
    config_->unset_local("remote." + name + ".url");
    config_->save();

    std::filesystem::path rdir = git_dir_ / "refs" / "remotes" / name;
    std::error_code ec;
    std::filesystem::remove_all(rdir, ec);
}

std::optional<std::string> Repository::get_remote_url(const std::string& name) const {
    return config_->get("remote." + name + ".url");
}

void Repository::set_remote_url(const std::string& name, const std::string& url) {
    if (!get_remote_url(name)) {
        throw std::runtime_error("No such remote: '" + name + "'");
    }
    config_->set_local("remote." + name + ".url", url);
    config_->save();
}

static void copy_tree_objects(const ObjectStore& src_store, ObjectStore& dst_store, const ObjectId& tree_id, size_t& count) {
    if (tree_id.is_zero()) return;
    if (dst_store.has_object(tree_id)) return;

    RawObject obj = src_store.read_object(tree_id);
    dst_store.write_object(obj.type, obj.data);
    count++;

    Tree tree = Tree::deserialize(obj.data);
    for (const auto& entry : tree.entries()) {
        if (entry.type == ObjectType::Tree) {
            copy_tree_objects(src_store, dst_store, entry.id, count);
        } else if (!dst_store.has_object(entry.id)) {
            RawObject bobj = src_store.read_object(entry.id);
            dst_store.write_object(bobj.type, bobj.data);
            count++;
        }
    }
}

static void copy_commit_and_ancestors(const ObjectStore& src_store, ObjectStore& dst_store, const ObjectId& commit_id, size_t& count) {
    if (commit_id.is_zero() || dst_store.has_object(commit_id)) return;

    std::queue<ObjectId> q;
    q.push(commit_id);

    while (!q.empty()) {
        ObjectId cur = q.front();
        q.pop();

        if (dst_store.has_object(cur)) continue;

        RawObject obj = src_store.read_object(cur);
        if (obj.type != ObjectType::Commit) continue;

        Commit commit = Commit::deserialize(obj.data);
        copy_tree_objects(src_store, dst_store, commit.tree_id, count);

        dst_store.write_object(obj.type, obj.data);
        count++;

        for (const auto& p : commit.parents) {
            if (!dst_store.has_object(p)) {
                q.push(p);
            }
        }
    }
}

static bool is_ancestor_commit(const ObjectStore& store, const ObjectId& ancestor_id, const ObjectId& descendant_id) {
    if (ancestor_id == descendant_id) return true;
    if (ancestor_id.is_zero() || descendant_id.is_zero()) return false;

    std::queue<ObjectId> q;
    std::unordered_set<ObjectId> visited;
    q.push(descendant_id);
    visited.insert(descendant_id);

    while (!q.empty()) {
        ObjectId cur = q.front();
        q.pop();

        if (cur == ancestor_id) return true;
        if (!store.has_object(cur)) continue;

        try {
            RawObject ro = store.read_object(cur);
            if (ro.type != ObjectType::Commit) continue;
            Commit com = Commit::deserialize(ro.data);
            for (const auto& p : com.parents) {
                if (visited.insert(p).second) {
                    q.push(p);
                }
            }
        } catch (...) {}
    }
    return false;
}

Repository::FetchResult Repository::fetch_remote(const std::string& remote_name, const std::string& branch_name) {
    auto url_opt = get_remote_url(remote_name);
    if (!url_opt) {
        throw std::runtime_error("Remote '" + remote_name + "' does not exist");
    }

    FetchResult result;
    result.remote = remote_name;

    Repository remote_repo = Repository::open(*url_opt);
    std::vector<std::string> branches_to_fetch;
    if (!branch_name.empty()) {
        branches_to_fetch.push_back(branch_name);
    } else {
        branches_to_fetch = remote_repo.refs().list_branches();
    }

    for (const auto& b : branches_to_fetch) {
        auto r_commit = remote_repo.refs().get_branch_commit(b);
        if (!r_commit) continue;

        auto local_tracking = refs_->get_remote_branch_commit(remote_name, b);
        if (local_tracking && *local_tracking == *r_commit) {
            continue;
        }

        FetchBranchResult fb;
        fb.branch = b;
        fb.old_commit = local_tracking.value_or(ObjectId::zero());
        fb.new_commit = *r_commit;
        fb.is_new = !local_tracking;
        fb.is_fast_forward = (!fb.is_new) && is_ancestor_commit(*objects_, fb.old_commit, fb.new_commit);

        copy_commit_and_ancestors(remote_repo.objects(), *objects_, *r_commit, result.objects_transferred);
        refs_->set_remote_branch_commit(remote_name, b, *r_commit);
        result.updated_branches.push_back(std::move(fb));
    }

    return result;
}

Repository::PushResult Repository::push_remote(const std::string& remote_name, const std::string& branch_name, bool force) {
    auto url_opt = get_remote_url(remote_name);
    if (!url_opt) {
        throw std::runtime_error("Remote '" + remote_name + "' does not exist");
    }

    std::string bname = branch_name;
    if (bname.empty()) {
        bname = refs_->get_current_branch();
        if (bname.empty()) {
            throw std::runtime_error("Cannot push from detached HEAD");
        }
    }

    auto local_commit = refs_->get_branch_commit(bname);
    if (!local_commit) {
        throw std::runtime_error("Branch '" + bname + "' does not exist locally or has no commits");
    }

    PushResult result;
    result.remote = remote_name;

    Repository remote_repo = Repository::open(*url_opt);
    auto remote_commit = remote_repo.refs().get_branch_commit(bname);

    PushBranchResult pb;
    pb.branch = bname;
    pb.old_commit = remote_commit.value_or(ObjectId::zero());
    pb.new_commit = *local_commit;
    pb.is_new = !remote_commit;

    if (remote_commit) {
        if (*remote_commit == *local_commit) {
            pb.is_fast_forward = true;
            result.results.push_back(std::move(pb));
            return result;
        }
        bool is_ff = is_ancestor_commit(*objects_, *remote_commit, *local_commit);
        pb.is_fast_forward = is_ff;
        if (!is_ff && !force) {
            pb.rejected = true;
            pb.message = "Updates were rejected because the remote contains work that you do not have locally. Run 'lgit pull' before pushing again.";
            result.results.push_back(std::move(pb));
            throw std::runtime_error(pb.message);
        }
    } else {
        pb.is_fast_forward = true;
    }

    copy_commit_and_ancestors(*objects_, remote_repo.objects(), *local_commit, result.objects_transferred);
    remote_repo.refs().set_branch_commit(bname, *local_commit);
    refs_->set_remote_branch_commit(remote_name, bname, *local_commit);

    // If remote repository is non-bare and has this branch checked out, update its tree if clean
    if (!remote_repo.is_bare() && remote_repo.refs().get_current_branch() == bname) {
        RepoStatus rstatus = remote_repo.compute_status();
        if (rstatus.is_clean()) {
            remote_repo.reset(*local_commit, ResetMode::Hard);
        }
    }

    result.results.push_back(std::move(pb));
    return result;
}

Repository::PullResult Repository::pull_remote(const std::string& remote_name, const std::string& branch_name) {
    std::string bname = branch_name;
    if (bname.empty()) {
        bname = refs_->get_current_branch();
        if (bname.empty()) {
            throw std::runtime_error("Cannot pull into detached HEAD");
        }
    }

    PullResult res;
    res.fetch_res = fetch_remote(remote_name, bname);

    auto tracking_commit = refs_->get_remote_branch_commit(remote_name, bname);
    if (!tracking_commit) {
        throw std::runtime_error("Cannot pull: no tracking branch found for '" + remote_name + "/" + bname + "'");
    }

    res.merge_res = merge(tracking_commit->to_hex());
    return res;
}

Repository Repository::clone_repository(const std::string& source_url_or_path, const std::filesystem::path& target_dir, const std::string& branch) {
    if (std::filesystem::exists(target_dir) && !std::filesystem::is_empty(target_dir)) {
        throw std::runtime_error("Destination path '" + target_dir.string() + "' already exists and is not an empty directory");
    }

    std::filesystem::path src_path(source_url_or_path);
    if (std::filesystem::is_regular_file(src_path)) {
        std::filesystem::create_directories(target_dir);
        Repository repo = Repository::init(target_dir, false, branch.empty() ? "main" : branch);
        repo.unbundle(src_path);

        std::string target_branch = branch;
        if (target_branch.empty()) {
            auto branches = repo.refs().list_branches();
            target_branch = branches.empty() ? "main" : branches.front();
        }
        if (auto cid = repo.refs().get_branch_commit(target_branch)) {
            repo.refs().set_head_symbolic(target_branch);
            repo.reset(*cid, ResetMode::Hard);
        }
        return repo;
    }

    Repository src_repo = Repository::open(source_url_or_path);
    std::string default_branch = branch;
    if (default_branch.empty()) {
        default_branch = src_repo.refs().get_current_branch();
        if (default_branch.empty()) {
            auto b_list = src_repo.refs().list_branches();
            default_branch = b_list.empty() ? "main" : b_list.front();
        }
    }

    std::filesystem::create_directories(target_dir);
    Repository repo = Repository::init(target_dir, false, default_branch);
    repo.add_remote("origin", std::filesystem::absolute(src_path).string());
    repo.fetch_remote("origin", default_branch);

    if (auto r_cid = repo.refs().get_remote_branch_commit("origin", default_branch)) {
        repo.refs().set_branch_commit(default_branch, *r_cid);
        repo.refs().set_head_symbolic(default_branch);
        repo.reset(*r_cid, ResetMode::Hard);
    }

    return repo;
}

void Repository::create_bundle(const std::filesystem::path& bundle_path, const std::vector<std::string>& revs) {
    std::vector<std::pair<std::string, ObjectId>> ref_list;
    if (revs.empty()) {
        for (const auto& b : refs_->list_branches()) {
            if (auto id = refs_->get_branch_commit(b)) {
                ref_list.push_back({"refs/heads/" + b, *id});
            }
        }
        for (const auto& t : refs_->list_tags()) {
            if (auto id = refs_->get_tag(t)) {
                ref_list.push_back({"refs/tags/" + t, *id});
            }
        }
        if (refs_->is_head_detached()) {
            ObjectId hid = refs_->resolve_head();
            if (hid) ref_list.push_back({"HEAD", hid});
        }
    } else {
        for (const auto& r : revs) {
            auto id = resolve_revision(r);
            if (!id) throw std::runtime_error("Cannot resolve revision '" + r + "' for bundle");
            ref_list.push_back({r, *id});
        }
    }

    if (ref_list.empty()) {
        throw std::runtime_error("No references to include in bundle");
    }

    std::unordered_set<ObjectId> reachable;
    for (const auto& [rname, rid] : ref_list) {
        mark_reachable_commit(*objects_, rid, reachable);
    }

    std::vector<uint8_t> buffer;
    static constexpr uint8_t MAGIC[8] = {'L', 'G', 'I', 'T', 'P', 'A', 'C', 'K'};
    buffer.insert(buffer.end(), std::begin(MAGIC), std::end(MAGIC));
    buffer.push_back(1);
    buffer.push_back(0);

    uint32_t num_refs = static_cast<uint32_t>(ref_list.size());
    for (int i = 0; i < 4; ++i) buffer.push_back(static_cast<uint8_t>((num_refs >> (i * 8)) & 0xFF));

    for (const auto& [rname, rid] : ref_list) {
        uint16_t nlen = static_cast<uint16_t>(rname.size());
        buffer.push_back(static_cast<uint8_t>(nlen & 0xFF));
        buffer.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        buffer.insert(buffer.end(), rname.begin(), rname.end());
        buffer.insert(buffer.end(), rid.bytes.begin(), rid.bytes.end());
    }

    uint32_t num_objs = static_cast<uint32_t>(reachable.size());
    for (int i = 0; i < 4; ++i) buffer.push_back(static_cast<uint8_t>((num_objs >> (i * 8)) & 0xFF));

    for (const auto& oid : reachable) {
        RawObject obj = objects_->read_object(oid);
        buffer.insert(buffer.end(), oid.bytes.begin(), oid.bytes.end());
        buffer.push_back(static_cast<uint8_t>(obj.type));
        uint64_t osize = static_cast<uint64_t>(obj.data.size());
        for (int i = 0; i < 8; ++i) buffer.push_back(static_cast<uint8_t>((osize >> (i * 8)) & 0xFF));
        buffer.insert(buffer.end(), obj.data.begin(), obj.data.end());
    }

    ObjectId checksum = hash_bytes(buffer.data(), buffer.size());
    buffer.insert(buffer.end(), checksum.bytes.begin(), checksum.bytes.end());

    fs_util::atomic_write_file(bundle_path, buffer.data(), buffer.size());
}

Repository::BundleInfo Repository::verify_bundle(const std::filesystem::path& bundle_path) {
    if (!std::filesystem::exists(bundle_path)) {
        throw std::runtime_error("Bundle file does not exist: " + bundle_path.string());
    }
    std::vector<uint8_t> data = fs_util::read_file_bytes(bundle_path);
    if (data.size() < 10 + 4 + 4 + 32) {
        throw std::runtime_error("Invalid or truncated bundle file: " + bundle_path.string());
    }

    static constexpr uint8_t MAGIC[8] = {'L', 'G', 'I', 'T', 'P', 'A', 'C', 'K'};
    if (std::memcmp(data.data(), MAGIC, 8) != 0) {
        throw std::runtime_error("Not a light-git bundle archive: " + bundle_path.string());
    }

    size_t payload_len = data.size() - 32;
    ObjectId expected = hash_bytes(data.data(), payload_len);
    ObjectId actual;
    std::memcpy(actual.bytes.data(), data.data() + payload_len, 32);
    if (expected != actual) {
        throw std::runtime_error("Bundle checksum verification failed (corrupted file): " + bundle_path.string());
    }

    BundleInfo info;
    size_t pos = 10;
    uint32_t num_refs = 0;
    for (int i = 0; i < 4; ++i) num_refs |= (static_cast<uint32_t>(data[pos + i]) << (i * 8));
    pos += 4;

    for (uint32_t i = 0; i < num_refs; ++i) {
        if (pos + 2 > payload_len) throw std::runtime_error("Truncated bundle references");
        uint16_t nlen = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        if (pos + nlen + 32 > payload_len) throw std::runtime_error("Truncated bundle reference entry");
        std::string rname(reinterpret_cast<const char*>(&data[pos]), nlen);
        pos += nlen;
        ObjectId rid;
        std::memcpy(rid.bytes.data(), &data[pos], 32);
        pos += 32;
        info.references[rname] = rid;
    }

    if (pos + 4 > payload_len) throw std::runtime_error("Truncated bundle objects count");
    uint32_t num_objs = 0;
    for (int i = 0; i < 4; ++i) num_objs |= (static_cast<uint32_t>(data[pos + i]) << (i * 8));
    info.object_count = num_objs;

    return info;
}

Repository::BundleInfo Repository::unbundle(const std::filesystem::path& bundle_path) {
    BundleInfo info = verify_bundle(bundle_path);
    std::vector<uint8_t> data = fs_util::read_file_bytes(bundle_path);

    size_t pos = 10;
    uint32_t num_refs = 0;
    for (int i = 0; i < 4; ++i) num_refs |= (static_cast<uint32_t>(data[pos + i]) << (i * 8));
    pos += 4;

    for (uint32_t i = 0; i < num_refs; ++i) {
        uint16_t nlen = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2 + nlen + 32;
    }

    uint32_t num_objs = 0;
    for (int i = 0; i < 4; ++i) num_objs |= (static_cast<uint32_t>(data[pos + i]) << (i * 8));
    pos += 4;

    for (uint32_t i = 0; i < num_objs; ++i) {
        if (pos + 32 + 1 + 8 > data.size() - 32) throw std::runtime_error("Truncated bundle object header");
        ObjectId oid;
        std::memcpy(oid.bytes.data(), &data[pos], 32);
        pos += 32;
        ObjectType otype = static_cast<ObjectType>(data[pos]);
        pos += 1;
        uint64_t osize = 0;
        for (int b = 0; b < 8; ++b) osize |= (static_cast<uint64_t>(data[pos + b]) << (b * 8));
        pos += 8;
        if (pos + osize > data.size() - 32) throw std::runtime_error("Truncated bundle object data");

        if (!objects_->has_object(oid)) {
            objects_->write_object(otype, &data[pos], static_cast<size_t>(osize));
        }
        pos += static_cast<size_t>(osize);
    }

    for (const auto& [rname, rid] : info.references) {
        if (rname.rfind("refs/heads/", 0) == 0) {
            refs_->set_branch_commit(rname.substr(11), rid);
        } else if (rname.rfind("refs/tags/", 0) == 0) {
            refs_->set_tag(rname.substr(10), rid);
        } else if (rname.find('/') == std::string::npos) {
            refs_->set_branch_commit(rname, rid);
        }
    }

    return info;
}

std::vector<Repository::WorktreeInfo> Repository::list_worktrees() const {
    std::vector<WorktreeInfo> result;

    WorktreeInfo main_wt;
    main_wt.name = work_dir_.filename().string();
    main_wt.path = work_dir_;
    main_wt.branch = refs_->is_head_detached() ? "detached HEAD" : refs_->get_current_branch();
    main_wt.commit_id = refs_->resolve_head();
    main_wt.is_main = true;
    main_wt.is_locked = false;
    result.push_back(std::move(main_wt));

    std::filesystem::path wt_dir = git_dir_ / "worktrees";
    if (std::filesystem::exists(wt_dir)) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(wt_dir, ec)) {
            if (!entry.is_directory(ec)) continue;
            std::string wt_name = entry.path().filename().string();
            std::filesystem::path gitdir_file = entry.path() / "gitdir";
            if (!std::filesystem::exists(gitdir_file)) continue;

            std::string target_link = fs_util::read_file_string(gitdir_file);
            while (!target_link.empty() && (target_link.back() == '\r' || target_link.back() == '\n' || target_link.back() == ' ')) target_link.pop_back();

            std::filesystem::path wt_path = std::filesystem::path(target_link).parent_path();
            WorktreeInfo wt;
            wt.name = wt_name;
            wt.path = wt_path;
            wt.is_main = false;
            wt.is_locked = std::filesystem::exists(entry.path() / "locked");

            std::filesystem::path head_f = entry.path() / "HEAD";
            if (std::filesystem::exists(head_f)) {
                std::string hcontent = fs_util::read_file_string(head_f);
                while (!hcontent.empty() && (hcontent.back() == '\r' || hcontent.back() == '\n' || hcontent.back() == ' ')) hcontent.pop_back();
                static constexpr std::string_view pfx = "ref: refs/heads/";
                if (hcontent.rfind(pfx, 0) == 0) {
                    wt.branch = hcontent.substr(pfx.size());
                    auto cid = refs_->get_branch_commit(wt.branch);
                    wt.commit_id = cid ? *cid : ObjectId::zero();
                } else {
                    wt.branch = "detached HEAD";
                    try { wt.commit_id = ObjectId::from_hex(hcontent); } catch (...) {}
                }
            }

            result.push_back(std::move(wt));
        }
    }

    return result;
}

void Repository::add_worktree(const std::filesystem::path& worktree_path, const std::string& branch_name) {
    if (branch_name.empty()) {
        throw std::runtime_error("Branch name must be specified when creating a worktree");
    }

    std::filesystem::path abs_path = std::filesystem::absolute(worktree_path);
    if (std::filesystem::exists(abs_path) && !std::filesystem::is_empty(abs_path)) {
        throw std::runtime_error("Worktree path '" + abs_path.string() + "' already exists and is not empty");
    }

    if (!refs_->branch_exists(branch_name)) {
        ObjectId head_id = refs_->resolve_head();
        if (!head_id) {
            throw std::runtime_error("Cannot create branch for worktree on empty repository");
        }
        create_branch(branch_name, head_id);
    }

    std::string wt_name = abs_path.filename().string();
    std::filesystem::path meta_dir = git_dir_ / "worktrees" / wt_name;
    std::filesystem::create_directories(meta_dir);

    fs_util::atomic_write_file(meta_dir / "commondir", std::filesystem::absolute(git_dir_).string() + "\n");
    fs_util::atomic_write_file(meta_dir / "gitdir", (abs_path / ".light-git").string() + "\n");
    fs_util::atomic_write_file(meta_dir / "HEAD", "ref: refs/heads/" + branch_name + "\n");

    std::filesystem::create_directories(abs_path);
    fs_util::atomic_write_file(abs_path / ".light-git", "gitdir: " + std::filesystem::absolute(meta_dir).string() + "\n");

    Repository wt_repo = Repository::open(abs_path);
    auto cid = refs_->get_branch_commit(branch_name);
    if (cid) {
        wt_repo.reset(*cid, ResetMode::Hard);
    }
}

void Repository::remove_worktree(const std::filesystem::path& worktree_path, bool force) {
    std::filesystem::path abs_path = std::filesystem::absolute(worktree_path);
    if (abs_path == std::filesystem::absolute(work_dir_)) {
        throw std::runtime_error("Cannot remove main worktree");
    }

    std::string wt_name = abs_path.filename().string();
    std::filesystem::path meta_dir = git_dir_ / "worktrees" / wt_name;

    if (!force && std::filesystem::exists(abs_path / ".light-git")) {
        try {
            Repository wt_repo = Repository::open(abs_path);
            if (!wt_repo.compute_status().is_clean()) {
                throw std::runtime_error("Worktree '" + abs_path.string() + "' contains modified or untracked files (use --force to discard)");
            }
        } catch (const std::exception&) {
            if (!force) throw;
        }
    }

    std::error_code ec;
    std::filesystem::remove_all(abs_path, ec);
    std::filesystem::remove_all(meta_dir, ec);
}

size_t Repository::prune_worktrees() {
    size_t count = 0;
    std::filesystem::path wt_dir = git_dir_ / "worktrees";
    if (!std::filesystem::exists(wt_dir)) return 0;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(wt_dir, ec)) {
        if (!entry.is_directory(ec)) continue;
        std::filesystem::path gitdir_file = entry.path() / "gitdir";
        bool prune_needed = false;
        if (!std::filesystem::exists(gitdir_file)) {
            prune_needed = true;
        } else {
            std::string target_link = fs_util::read_file_string(gitdir_file);
            while (!target_link.empty() && (target_link.back() == '\r' || target_link.back() == '\n' || target_link.back() == ' ')) target_link.pop_back();
            if (!std::filesystem::exists(target_link)) {
                prune_needed = true;
            }
        }

        if (prune_needed) {
            std::filesystem::remove_all(entry.path(), ec);
            count++;
        }
    }
    return count;
}

} // namespace light_git
