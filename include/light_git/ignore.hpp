#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace light_git {

struct IgnoreMatchResult {
    bool ignored{false};
    std::string matched_pattern;
    std::string source_file;
    size_t line_number{0};
};

struct IgnoreRule {
    std::string raw_pattern;
    std::string source_file;
    size_t line_number{0};
    bool is_negation{false};
    bool is_dir_only{false};
    bool is_anchored{false};
    std::string pattern;

    static std::optional<IgnoreRule> parse(std::string_view line, const std::string& source = ".light-gitignore", size_t line_num = 0);
    bool matches(std::string_view path, bool is_directory) const;
};

class IgnoreMatcher {
public:
    IgnoreMatcher() = default;

    void add_rule(const IgnoreRule& rule);
    void load_from_file(const std::filesystem::path& path);
    void load_from_string(std::string_view content, const std::string& source = "<memory>");

    // Determines if path is ignored. Checks all rules in order (last matching rule wins)
    IgnoreMatchResult check(std::string_view path, bool is_directory) const;
    bool is_ignored(std::string_view path, bool is_directory) const;

    const std::vector<IgnoreRule>& rules() const { return rules_; }
    void clear() { rules_.clear(); }

private:
    std::vector<IgnoreRule> rules_;
};

} // namespace light_git
