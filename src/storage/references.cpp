#include "light_git/references.hpp"
#include "light_git/filesystem.hpp"
#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace light_git {

References::References(std::filesystem::path git_dir)
    : git_dir_(std::move(git_dir)) {
}

static std::string trim_ref(std::string_view sv) {
    while (!sv.empty() && (sv.back() == '\r' || sv.back() == '\n' || sv.back() == ' ')) {
        sv.remove_suffix(1);
    }
    while (!sv.empty() && sv.front() == ' ') {
        sv.remove_prefix(1);
    }
    return std::string(sv);
}

bool References::is_head_detached() const {
    if (!std::filesystem::exists(head_file())) {
        return false;
    }
    std::string content = trim_ref(fs_util::read_file_string(head_file()));
    return (content.rfind("ref: ", 0) != 0);
}

std::string References::get_current_branch() const {
    if (!std::filesystem::exists(head_file())) {
        return "";
    }
    std::string content = trim_ref(fs_util::read_file_string(head_file()));
    static constexpr std::string_view prefix = "ref: refs/heads/";
    if (content.rfind(prefix, 0) == 0) {
        return content.substr(prefix.size());
    }
    return "";
}

ObjectId References::resolve_head() const {
    if (!std::filesystem::exists(head_file())) {
        return ObjectId::zero();
    }
    std::string content = trim_ref(fs_util::read_file_string(head_file()));
    if (content.empty()) {
        return ObjectId::zero();
    }
    static constexpr std::string_view prefix = "ref: refs/heads/";
    if (content.rfind(prefix, 0) == 0) {
        std::string branch = content.substr(prefix.size());
        auto opt = get_branch_commit(branch);
        return opt ? *opt : ObjectId::zero();
    }

    try {
        return ObjectId::from_hex(content);
    } catch (const std::exception& e) {
        throw std::runtime_error("corrupted HEAD reference in '" + head_file().string() + "': " + e.what());
    }
}

void References::set_head_symbolic(const std::string& branch_name) {
    std::string content = "ref: refs/heads/" + branch_name + "\n";
    fs_util::atomic_write_file(head_file(), content);
}

void References::set_head_detached(const ObjectId& commit_id) {
    std::string content = commit_id.to_hex() + "\n";
    fs_util::atomic_write_file(head_file(), content);
}

std::optional<ObjectId> References::get_branch_commit(const std::string& branch_name) const {
    std::filesystem::path bpath = heads_dir() / branch_name;
    if (!std::filesystem::exists(bpath)) {
        return std::nullopt;
    }
    std::string content = trim_ref(fs_util::read_file_string(bpath));
    if (content.empty()) {
        throw std::runtime_error("corrupted reference 'refs/heads/" + branch_name + "': file is empty");
    }
    try {
        return ObjectId::from_hex(content);
    } catch (const std::exception& e) {
        throw std::runtime_error("corrupted reference 'refs/heads/" + branch_name + "': " + e.what());
    }
}

void References::set_branch_commit(const std::string& branch_name, const ObjectId& commit_id) {
    std::filesystem::path bpath = heads_dir() / branch_name;
    std::string content = commit_id.to_hex() + "\n";
    fs_util::atomic_write_file(bpath, content);
}

bool References::delete_branch(const std::string& branch_name) {
    std::filesystem::path bpath = heads_dir() / branch_name;
    std::error_code ec;
    return std::filesystem::remove(bpath, ec);
}

bool References::branch_exists(const std::string& branch_name) const {
    return std::filesystem::exists(heads_dir() / branch_name);
}

std::vector<std::string> References::list_branches() const {
    std::vector<std::string> branches;
    if (!std::filesystem::exists(heads_dir())) {
        return branches;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(heads_dir(), ec)) {
        if (entry.is_regular_file(ec)) {
            auto rel = entry.path().lexically_relative(heads_dir());
            branches.push_back(fs_util::normalize_path(rel));
        }
    }
    std::sort(branches.begin(), branches.end());
    return branches;
}

std::optional<ObjectId> References::get_tag(const std::string& tag_name) const {
    std::filesystem::path tpath = tags_dir() / tag_name;
    if (!std::filesystem::exists(tpath)) {
        return std::nullopt;
    }
    std::string content = trim_ref(fs_util::read_file_string(tpath));
    if (content.empty()) {
        throw std::runtime_error("corrupted tag reference 'refs/tags/" + tag_name + "': file is empty");
    }
    try {
        return ObjectId::from_hex(content);
    } catch (const std::exception& e) {
        throw std::runtime_error("corrupted tag reference 'refs/tags/" + tag_name + "': " + e.what());
    }
}

void References::set_tag(const std::string& tag_name, const ObjectId& commit_id) {
    std::filesystem::path tpath = tags_dir() / tag_name;
    std::string content = commit_id.to_hex() + "\n";
    fs_util::atomic_write_file(tpath, content);
}

bool References::delete_tag(const std::string& tag_name) {
    std::filesystem::path tpath = tags_dir() / tag_name;
    std::error_code ec;
    return std::filesystem::remove(tpath, ec);
}

std::vector<std::string> References::list_tags() const {
    std::vector<std::string> tags;
    if (!std::filesystem::exists(tags_dir())) {
        return tags;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tags_dir(), ec)) {
        if (entry.is_regular_file(ec)) {
            auto rel = entry.path().lexically_relative(tags_dir());
            tags.push_back(fs_util::normalize_path(rel));
        }
    }
    std::sort(tags.begin(), tags.end());
    return tags;
}

std::vector<std::pair<std::string, ObjectId>> References::list_all_references() const {
    std::vector<std::pair<std::string, ObjectId>> refs;

    // Branches
    for (const auto& b : list_branches()) {
        auto id = get_branch_commit(b);
        if (id) {
            refs.push_back({"refs/heads/" + b, *id});
        }
    }

    // Tags
    for (const auto& t : list_tags()) {
        auto id = get_tag(t);
        if (id) {
            refs.push_back({"refs/tags/" + t, *id});
        }
    }

    // Detached HEAD
    if (is_head_detached()) {
        ObjectId head_id = resolve_head();
        if (head_id) {
            refs.push_back({"HEAD", head_id});
        }
    }

    return refs;
}

} // namespace light_git
