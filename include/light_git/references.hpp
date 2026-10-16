#pragma once

#include "light_git/types.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace light_git {

class References {
public:
    explicit References(std::filesystem::path git_dir, std::filesystem::path head_file = {});

    // HEAD management
    bool is_head_detached() const;
    std::string get_current_branch() const; // Empty string if detached
    ObjectId resolve_head() const;          // Returns zero if branch is unborn / no commits yet
    void set_head_symbolic(const std::string& branch_name);
    void set_head_detached(const ObjectId& commit_id);

    // Branch operations
    std::optional<ObjectId> get_branch_commit(const std::string& branch_name) const;
    void set_branch_commit(const std::string& branch_name, const ObjectId& commit_id);
    bool delete_branch(const std::string& branch_name);
    std::vector<std::string> list_branches() const;
    bool branch_exists(const std::string& branch_name) const;

    // Tag operations
    std::optional<ObjectId> get_tag(const std::string& tag_name) const;
    void set_tag(const std::string& tag_name, const ObjectId& commit_id);
    bool delete_tag(const std::string& tag_name);
    std::vector<std::string> list_tags() const;

    // Remote tracking operations
    std::optional<ObjectId> get_remote_branch_commit(const std::string& remote_name, const std::string& branch_name) const;
    void set_remote_branch_commit(const std::string& remote_name, const std::string& branch_name, const ObjectId& commit_id);
    bool delete_remote_branch(const std::string& remote_name, const std::string& branch_name);
    std::vector<std::string> list_remote_branches(const std::string& remote_name) const;
    std::vector<std::pair<std::string, std::string>> list_all_remote_branches() const;

    // All refs for gc & fsck
    std::vector<std::pair<std::string, ObjectId>> list_all_references() const;

private:
    std::filesystem::path git_dir_;
    std::filesystem::path head_file_;
    std::filesystem::path head_file() const { return head_file_.empty() ? (git_dir_ / "HEAD") : head_file_; }
    std::filesystem::path heads_dir() const { return git_dir_ / "refs" / "heads"; }
    std::filesystem::path tags_dir() const { return git_dir_ / "refs" / "tags"; }
    std::filesystem::path remotes_dir() const { return git_dir_ / "refs" / "remotes"; }
};

} // namespace light_git
