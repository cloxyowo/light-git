#include "light_git/config.hpp"
#include "light_git/filesystem.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace light_git {

static std::string trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

static std::string to_lower(std::string_view sv) {
    std::string result(sv);
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::pair<std::string, std::string> Config::split_key(const std::string& full_key) {
    size_t dot = full_key.find('.');
    if (dot == std::string::npos) {
        return {"", to_lower(full_key)};
    }
    return {to_lower(full_key.substr(0, dot)), to_lower(full_key.substr(dot + 1))};
}

std::filesystem::path Config::get_global_config_path() {
#if defined(_WIN32)
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        return std::filesystem::path(userprofile) / ".light-gitconfig";
    }
    const char* homedrive = std::getenv("HOMEDRIVE");
    const char* homepath = std::getenv("HOMEPATH");
    if (homedrive && homepath) {
        return (std::filesystem::path(homedrive) / homepath) / ".light-gitconfig";
    }
    return ".light-gitconfig";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".light-gitconfig";
    }
    return ".light-gitconfig";
#endif
}

void Config::load_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string current_section;
    std::string line;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = to_lower(trim(trimmed.substr(1, trimmed.size() - 2)));
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = to_lower(trim(trimmed.substr(0, eq_pos)));
            std::string val = trim(trimmed.substr(eq_pos + 1));
            // Unquote value if quoted
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                                    (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }
            std::string full_key = current_section.empty() ? key : (current_section + "." + key);
            entries_[full_key] = val;
        }
    }
}

void Config::save_file(const std::filesystem::path& path) const {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    // Group entries by section
    std::map<std::string, std::map<std::string, std::string>> sections;
    for (const auto& [k, v] : entries_) {
        auto [sec, subkey] = split_key(k);
        sections[sec][subkey] = v;
    }

    std::ostringstream ss;
    bool first = true;
    for (const auto& [sec, kvs] : sections) {
        if (!first) ss << "\n";
        first = false;

        if (!sec.empty()) {
            ss << "[" << sec << "]\n";
        }
        for (const auto& [k, v] : kvs) {
            ss << "\t" << k << " = " << v << "\n";
        }
    }

    fs_util::atomic_write_file(path, ss.str());
}

std::optional<std::string> Config::get(const std::string& key) const {
    auto it = entries_.find(to_lower(key));
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string Config::get_or(const std::string& key, const std::string& default_val) const {
    auto val = get(key);
    return val ? *val : default_val;
}

bool Config::get_bool(const std::string& key, bool default_val) const {
    auto val = get(key);
    if (!val) return default_val;
    std::string s = to_lower(*val);
    if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
    if (s == "false" || s == "0" || s == "no" || s == "off") return false;
    return default_val;
}

int Config::get_int(const std::string& key, int default_val) const {
    auto val = get(key);
    if (!val) return default_val;
    try {
        return std::stoi(*val);
    } catch (...) {
        return default_val;
    }
}

void Config::set(const std::string& key, const std::string& value) {
    entries_[to_lower(key)] = value;
}

bool Config::unset(const std::string& key) {
    return entries_.erase(to_lower(key)) > 0;
}

std::map<std::string, std::string> Config::get_all() const {
    return entries_;
}

void RepoConfig::load(const std::filesystem::path& repo_config_path) {
    local_path_ = repo_config_path;
    global_config_.load_file(Config::get_global_config_path());
    local_config_.load_file(repo_config_path);
}

void RepoConfig::save() {
    if (!local_path_.empty()) {
        local_config_.save_file(local_path_);
    }
}

std::optional<std::string> RepoConfig::get(const std::string& key) const {
    auto local_val = local_config_.get(key);
    if (local_val) return local_val;
    return global_config_.get(key);
}

std::string RepoConfig::get_or(const std::string& key, const std::string& default_val) const {
    auto val = get(key);
    return val ? *val : default_val;
}

bool RepoConfig::get_bool(const std::string& key, bool default_val) const {
    auto val = get(key);
    if (!val) return default_val;
    std::string s = to_lower(*val);
    if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
    if (s == "false" || s == "0" || s == "no" || s == "off") return false;
    return default_val;
}

void RepoConfig::set_local(const std::string& key, const std::string& value) {
    local_config_.set(key, value);
    save();
}

void RepoConfig::set_global(const std::string& key, const std::string& value) {
    auto path = Config::get_global_config_path();
    global_config_.set(key, value);
    global_config_.save_file(path);
}

bool RepoConfig::unset_local(const std::string& key) {
    bool ok = local_config_.unset(key);
    if (ok) save();
    return ok;
}

bool RepoConfig::unset_global(const std::string& key) {
    auto path = Config::get_global_config_path();
    bool ok = global_config_.unset(key);
    if (ok) global_config_.save_file(path);
    return ok;
}

std::map<std::string, std::string> RepoConfig::get_all_merged() const {
    auto merged = global_config_.get_all();
    for (const auto& [k, v] : local_config_.get_all()) {
        merged[k] = v;
    }
    return merged;
}

} // namespace light_git
