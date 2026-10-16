#pragma once

#include "light_git/types.hpp"
#include "light_git/object_store.hpp"
#include "light_git/references.hpp"
#include "light_git/index.hpp"
#include "light_git/config.hpp"
#include "light_git/ignore.hpp"
#include "light_git/diff.hpp"
#include "light_git/tree.hpp"
#include "light_git/commit.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace light_git {

enum class ResetMode {
    Soft,
    Mixed,
    Hard
};

struct RepoStatus {
    std::string branch_name;
    bool is_detached{false};
    ObjectId head_commit;

    std::vector<std::string> staged_new;
    std::vector<std::string> staged_modified;
    std::vector<std::string> staged_deleted;

    std::vector<std::string> unstaged_modified;
    std::vector<std::string> unstaged_deleted;

    std::vector<std::string> untracked;

    [[nodiscard]] bool is_clean() const {
        return staged_new.empty() && staged_modified.empty() && staged_deleted.empty() &&
               unstaged_modified.empty() && unstaged_deleted.empty() && untracked.empty();
    }
};

class Repository {
public:
    static Repository init(const std::filesystem::path& path, bool bare = false, const std::string& default_branch = "main");
    static Repository open(const std::filesystem::path& start_path);
    static std::optional<std::filesystem::path> find_repository_root(const std::filesystem::path& start_path);

    const std::filesystem::path& work_dir() const { return work_dir_; }
    const std::filesystem::path& git_dir() const { return git_dir_; }
    bool is_bare() const { return is_bare_; }

    ObjectStore& objects() { return *objects_; }
    const ObjectStore& objects() const { return *objects_; }

    References& refs() { return *refs_; }
    const References& refs() const { return *refs_; }

    Index& index() { return *index_; }
    const Index& index() const { return *index_; }

    RepoConfig& config() { return *config_; }
    const RepoConfig& config() const { return *config_; }

    IgnoreMatcher& ignores() { return ignores_; }
    const IgnoreMatcher& ignores() const { return ignores_; }

    // Status
    RepoStatus compute_status();

    // Staging
    void stage_path(const std::string& rel_or_abs_path);
    void stage_all();

    // Commits
    ObjectId create_commit(const std::string& message, bool amend = false);

    // Branches & Switching
    void switch_branch(const std::string& branch_name, bool create_new = false);
    void create_branch(const std::string& branch_name, const ObjectId& start_point = ObjectId::zero());
    void delete_branch(const std::string& branch_name, bool force = false);

    // Restore & Reset
    void restore_file(const std::string& rel_path, bool staged, const std::optional<ObjectId>& commit_id = std::nullopt);
    void reset(const ObjectId& target_commit, ResetMode mode);

    // Rm & Mv
    void remove_path(const std::string& rel_path, bool cached_only = false);
    void move_path(const std::string& src_path, const std::string& dest_path);

    // Diffs
    std::vector<FileDiff> diff_working_vs_index();
    std::vector<FileDiff> diff_index_vs_head();
    std::vector<FileDiff> diff_commit(const ObjectId& commit_id);
    std::vector<FileDiff> diff_commits(const ObjectId& old_commit_id, const ObjectId& new_commit_id);
    std::vector<FileDiff> diff_working_vs_commit(const ObjectId& commit_id);

    // Maintenance
    struct GcResult {
        size_t total_objects{0};
        size_t kept_objects{0};
        size_t removed_objects{0};
    };
    GcResult garbage_collect();

    struct FsckIssue {
        enum class Severity { Warning, Error } severity;
        std::string message;
    };
    std::vector<FsckIssue> verify_integrity();

    // Build tree hierarchy from index
    ObjectId write_tree_from_index();

    // Revisions
    std::optional<ObjectId> resolve_revision(const std::string& rev) const;

    // Tags
    void create_tag(const std::string& name, const ObjectId& target = ObjectId::zero(), const std::string& message = "");
    void delete_tag(const std::string& name);
    std::vector<std::string> list_tags() const;

    // Stash
    struct StashEntry {
        size_t index{0};
        ObjectId commit_id;
        std::string message;
        std::string branch;
        int64_t timestamp{0};
    };
    std::vector<StashEntry> list_stashes() const;
    ObjectId stash_push(const std::string& message = "");
    void stash_apply(size_t index = 0);
    void stash_pop(size_t index = 0);
    void stash_drop(size_t index = 0);
    void stash_clear();

    // Merge
    struct MergeResult {
        bool is_fast_forward{false};
        bool already_up_to_date{false};
        bool has_conflicts{false};
        ObjectId commit_id;
        std::vector<std::string> conflict_files;
        std::vector<std::string> merged_files;
        std::string message;
    };
    MergeResult merge(const std::string& branch_or_commit);

    // Cherry-pick
    ObjectId cherry_pick(const ObjectId& commit_id);

    // Git Export Bridge
    std::string export_git_fastimport() const;
    void export_to_git(const std::filesystem::path& target_git_dir);

    // Remotes
    struct RemoteInfo {
        std::string name;
        std::string url;
    };
    std::vector<RemoteInfo> list_remotes() const;
    void add_remote(const std::string& name, const std::string& url);
    void remove_remote(const std::string& name);
    std::optional<std::string> get_remote_url(const std::string& name) const;
    void set_remote_url(const std::string& name, const std::string& url);

    // Remote Synchronization (Fetch, Push, Pull)
    struct FetchBranchResult {
        std::string branch;
        ObjectId old_commit;
        ObjectId new_commit;
        bool is_new{false};
        bool is_fast_forward{false};
    };
    struct FetchResult {
        std::string remote;
        std::vector<FetchBranchResult> updated_branches;
        size_t objects_transferred{0};
    };
    FetchResult fetch_remote(const std::string& remote_name = "origin", const std::string& branch_name = "");

    struct PushBranchResult {
        std::string branch;
        ObjectId old_commit;
        ObjectId new_commit;
        bool is_new{false};
        bool is_fast_forward{false};
        bool rejected{false};
        std::string message;
    };
    struct PushResult {
        std::string remote;
        std::vector<PushBranchResult> results;
        size_t objects_transferred{0};
    };
    PushResult push_remote(const std::string& remote_name = "origin", const std::string& branch_name = "", bool force = false);

    struct PullResult {
        FetchResult fetch_res;
        MergeResult merge_res;
    };
    PullResult pull_remote(const std::string& remote_name = "origin", const std::string& branch_name = "");

    // Clone
    static Repository clone_repository(const std::string& source_url_or_path, const std::filesystem::path& target_dir, const std::string& branch = "");

    // Bundles (.lgitpack)
    struct BundleInfo {
        std::map<std::string, ObjectId> references;
        size_t object_count{0};
    };
    void create_bundle(const std::filesystem::path& bundle_path, const std::vector<std::string>& revs = {});
    static BundleInfo verify_bundle(const std::filesystem::path& bundle_path);
    BundleInfo unbundle(const std::filesystem::path& bundle_path);

    // Worktrees
    struct WorktreeInfo {
        std::string name;
        std::filesystem::path path;
        std::string branch;
        ObjectId commit_id;
        bool is_main{false};
        bool is_locked{false};
    };
    std::vector<WorktreeInfo> list_worktrees() const;
    void add_worktree(const std::filesystem::path& worktree_path, const std::string& branch_name = "");
    void remove_worktree(const std::filesystem::path& worktree_path, bool force = false);
    size_t prune_worktrees();

private:
    Repository(std::filesystem::path work_dir, std::filesystem::path git_dir, bool bare);

    void load_ignores();

    std::filesystem::path work_dir_;
    std::filesystem::path git_dir_;
    bool is_bare_{false};

    std::unique_ptr<ObjectStore> objects_;
    std::unique_ptr<References> refs_;
    std::unique_ptr<Index> index_;
    std::unique_ptr<RepoConfig> config_;
    IgnoreMatcher ignores_;
};

} // namespace light_git
