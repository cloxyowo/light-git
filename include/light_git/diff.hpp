#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <cstdint>

namespace light_git {

enum class DiffLineType {
    Context,
    Addition,
    Deletion
};

struct DiffLine {
    DiffLineType type{DiffLineType::Context};
    std::string text;
};

struct DiffHunk {
    size_t old_start{0};
    size_t old_count{0};
    size_t new_start{0};
    size_t new_count{0};
    std::vector<DiffLine> lines;
};

struct FileDiff {
    std::string old_path;
    std::string new_path;
    bool is_binary{false};
    bool is_new_file{false};
    bool is_deleted_file{false};
    std::vector<DiffHunk> hunks;

    size_t insertions{0};
    size_t deletions{0};

    [[nodiscard]] bool empty() const {
        return !is_binary && !is_new_file && !is_deleted_file && hunks.empty();
    }
};

class DiffEngine {
public:
    static FileDiff compute_diff(std::string_view old_content, std::string_view new_content,
                                 const std::string& old_path, const std::string& new_path,
                                 size_t context_lines = 3);

    static std::string format_diff(const FileDiff& diff, bool color = true);
    static std::string format_stats(const std::vector<FileDiff>& diffs);
};

} // namespace light_git
