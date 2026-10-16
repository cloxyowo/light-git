#include "light_git/ignore.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace light_git {

namespace {

// Helper: match pattern against text where '*' matches anything except '/', '?' matches one non-'/'
bool match_segment(std::string_view pat, std::string_view text) {
    size_t px = 0;
    size_t tx = 0;
    size_t next_px = 0;
    size_t next_tx = 0;
    bool has_star = false;

    while (tx < text.size() || px < pat.size()) {
        if (px < pat.size()) {
            char c = pat[px];
            if (c == '*') {
                has_star = true;
                px++;
                next_px = px;
                next_tx = tx + 1;
                continue;
            }
            if (tx < text.size() && (c == '?' || c == text[tx])) {
                px++;
                tx++;
                continue;
            }
        }
        if (has_star && next_tx <= text.size()) {
            px = next_px;
            tx = next_tx++;
            continue;
        }
        return false;
    }
    return true;
}

// Split path by '/'
std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start < path.size()) {
        while (start < path.size() && path[start] == '/') {
            start++;
        }
        if (start >= path.size()) break;
        size_t end = path.find('/', start);
        if (end == std::string_view::npos) {
            parts.push_back(path.substr(start));
            break;
        }
        parts.push_back(path.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

// Match parts against glob parts including "**"
bool match_parts(const std::vector<std::string_view>& pat_parts, size_t pi,
                 const std::vector<std::string_view>& text_parts, size_t ti) {
    while (pi < pat_parts.size() && ti < text_parts.size()) {
        if (pat_parts[pi] == "**") {
            // "**" matches 0 or more directories
            if (pi + 1 == pat_parts.size()) {
                return true; // trailing ** matches everything
            }
            for (size_t k = ti; k <= text_parts.size(); ++k) {
                if (match_parts(pat_parts, pi + 1, text_parts, k)) {
                    return true;
                }
            }
            return false;
        }
        if (!match_segment(pat_parts[pi], text_parts[ti])) {
            return false;
        }
        pi++;
        ti++;
    }

    while (pi < pat_parts.size() && pat_parts[pi] == "**") {
        pi++;
    }

    return (pi == pat_parts.size() && ti == text_parts.size());
}

} // namespace

std::optional<IgnoreRule> IgnoreRule::parse(std::string_view line, const std::string& source, size_t line_num) {
    // Strip leading and trailing whitespace
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
        line.remove_prefix(1);
    }
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
        line.remove_suffix(1);
    }

    if (line.empty() || line.front() == '#') {
        return std::nullopt;
    }

    IgnoreRule rule;
    rule.raw_pattern = std::string(line);
    rule.source_file = source;
    rule.line_number = line_num;

    if (line.front() == '!') {
        rule.is_negation = true;
        line.remove_prefix(1);
    }

    if (!line.empty() && line.back() == '/') {
        rule.is_dir_only = true;
        line.remove_suffix(1);
    }

    if (!line.empty() && line.front() == '/') {
        rule.is_anchored = true;
        line.remove_prefix(1);
    }

    rule.pattern = std::string(line);
    return rule;
}

bool IgnoreRule::matches(std::string_view path, bool is_directory) const {
    if (is_dir_only && !is_directory) {
        return false;
    }

    // Strip leading '/'
    while (!path.empty() && path.front() == '/') {
        path.remove_prefix(1);
    }
    // Strip trailing '/'
    while (!path.empty() && path.back() == '/') {
        path.remove_suffix(1);
    }

    if (path.empty()) {
        return false;
    }

    std::vector<std::string_view> path_parts = split_path(path);
    std::vector<std::string_view> pat_parts = split_path(pattern);

    if (pat_parts.empty()) {
        return false;
    }

    // If pattern does not contain a slash and is not anchored, match against basename or any directory name
    bool has_slash = (pattern.find('/') != std::string::npos);
    if (!has_slash && !is_anchored) {
        // Match against file basename
        if (match_segment(pattern, path_parts.back())) {
            return true;
        }
        // Also if it's a directory rule or matches any folder along the path
        for (const auto& part : path_parts) {
            if (match_segment(pattern, part)) {
                return true;
            }
        }
        return false;
    }

    // Anchored or contains slash: match from root
    return match_parts(pat_parts, 0, path_parts, 0);
}

void IgnoreMatcher::add_rule(const IgnoreRule& rule) {
    rules_.push_back(rule);
}

void IgnoreMatcher::load_from_string(std::string_view content, const std::string& source) {
    std::istringstream stream((std::string(content)));
    std::string line;
    size_t line_no = 0;
    while (std::getline(stream, line)) {
        line_no++;
        auto rule = IgnoreRule::parse(line, source, line_no);
        if (rule) {
            rules_.push_back(*rule);
        }
    }
}

void IgnoreMatcher::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    size_t line_no = 0;
    while (std::getline(file, line)) {
        line_no++;
        auto rule = IgnoreRule::parse(line, path.string(), line_no);
        if (rule) {
            rules_.push_back(*rule);
        }
    }
}

IgnoreMatchResult IgnoreMatcher::check(std::string_view path, bool is_directory) const {
    IgnoreMatchResult result;
    result.ignored = false;

    // Check rules in order; last match wins
    for (const auto& rule : rules_) {
        if (rule.matches(path, is_directory)) {
            result.ignored = !rule.is_negation;
            result.matched_pattern = rule.raw_pattern;
            result.source_file = rule.source_file;
            result.line_number = rule.line_number;
        }
    }

    return result;
}

bool IgnoreMatcher::is_ignored(std::string_view path, bool is_directory) const {
    return check(path, is_directory).ignored;
}

} // namespace light_git
