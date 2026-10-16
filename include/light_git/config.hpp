#pragma once

#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <optional>

namespace light_git {

class Config {
public:
    Config() = default;

    void load_file(const std::filesystem::path& path);
    void save_file(const std::filesystem::path& path) const;

    std::optional<std::string> get(const std::string& key) const;
    std::string get_or(const std::string& key, const std::string& default_val) const;
    bool get_bool(const std::string& key, bool default_val = false) const;
    int get_int(const std::string& key, int default_val = 0) const;

    void set(const std::string& key, const std::string& value);
    bool unset(const std::string& key);

    std::map<std::string, std::string> get_all() const;

    static std::filesystem::path get_global_config_path();

    // Parse section.key
    static std::pair<std::string, std::string> split_key(const std::string& full_key);

private:
    std::map<std::string, std::string> entries_;
};

// Merged configuration: user global + repo local
class RepoConfig {
public:
    void load(const std::filesystem::path& repo_config_path);
    void save();

    std::optional<std::string> get(const std::string& key) const;
    std::string get_or(const std::string& key, const std::string& default_val) const;
    bool get_bool(const std::string& key, bool default_val = false) const;

    void set_local(const std::string& key, const std::string& value);
    void set_global(const std::string& key, const std::string& value);
    bool unset_local(const std::string& key);
    bool unset_global(const std::string& key);

    std::map<std::string, std::string> get_all_merged() const;

    const std::filesystem::path& local_path() const { return local_path_; }

private:
    Config global_config_;
    Config local_config_;
    std::filesystem::path local_path_;
};

} // namespace light_git
