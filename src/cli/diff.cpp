#include "light_git/diff.hpp"
#include "light_git/terminal.hpp"
#include "light_git/filesystem.hpp"
#include <sstream>
#include <algorithm>
#include <vector>

namespace light_git {

namespace {

std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            std::string line(text.substr(start));
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
            break;
        }
        std::string line(text.substr(start, end - start));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

// Myers Diff Implementation
struct Edit {
    DiffLineType type;
    std::string text;
};

std::vector<Edit> myers_diff(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    int max = n + m;

    if (max == 0) {
        return {};
    }

    std::vector<int> v(2 * max + 1, 0);
    std::vector<std::vector<int>> trace;

    for (int d = 0; d <= max; ++d) {
        trace.push_back(v);
        for (int k = -d; k <= d; k += 2) {
            int k_idx = k + max;
            int x = 0;
            if (k == -d || (k != d && v[k_idx - 1] < v[k_idx + 1])) {
                x = v[k_idx + 1];
            } else {
                x = v[k_idx - 1] + 1;
            }
            int y = x - k;

            while (x < n && y < m && a[x] == b[y]) {
                x++;
                y++;
            }
            v[k_idx] = x;

            if (x >= n && y >= m) {
                // Backtrack
                std::vector<Edit> edits;
                int curr_x = n;
                int curr_y = m;

                for (int step = d; step >= 0; --step) {
                    const auto& prev_v = trace[step];
                    int curr_k = curr_x - curr_y;
                    int prev_k = 0;

                    if (curr_k == -step || (curr_k != step && prev_v[curr_k - 1 + max] < prev_v[curr_k + 1 + max])) {
                        prev_k = curr_k + 1;
                    } else {
                        prev_k = curr_k - 1;
                    }

                    int prev_x = prev_v[prev_k + max];
                    int prev_y = prev_x - prev_k;

                    while (curr_x > prev_x && curr_y > prev_y) {
                        edits.push_back({DiffLineType::Context, a[curr_x - 1]});
                        curr_x--;
                        curr_y--;
                    }

                    if (step > 0) {
                        if (curr_x == prev_x) {
                            edits.push_back({DiffLineType::Addition, b[prev_y]});
                            curr_y--;
                        } else {
                            edits.push_back({DiffLineType::Deletion, a[prev_x]});
                            curr_x--;
                        }
                    }
                }

                std::reverse(edits.begin(), edits.end());
                return edits;
            }
        }
    }
    return {};
}

} // namespace

FileDiff DiffEngine::compute_diff(std::string_view old_content, std::string_view new_content,
                                 const std::string& old_path, const std::string& new_path,
                                 size_t context_lines) {
    FileDiff fd;
    fd.old_path = old_path;
    fd.new_path = new_path;

    if (fs_util::is_binary(old_content.data(), old_content.size()) ||
        fs_util::is_binary(new_content.data(), new_content.size())) {
        fd.is_binary = true;
        return fd;
    }

    auto a_lines = split_lines(old_content);
    auto b_lines = split_lines(new_content);

    if (old_content.empty() && !new_content.empty()) {
        fd.is_new_file = true;
    } else if (!old_content.empty() && new_content.empty()) {
        fd.is_deleted_file = true;
    }

    std::vector<Edit> edits = myers_diff(a_lines, b_lines);

    for (const auto& e : edits) {
        if (e.type == DiffLineType::Addition) fd.insertions++;
        if (e.type == DiffLineType::Deletion) fd.deletions++;
    }

    // If no changes, return empty diff
    if (fd.insertions == 0 && fd.deletions == 0) {
        return fd;
    }

    // Group edits into hunks with context
    std::vector<size_t> change_indices;
    for (size_t i = 0; i < edits.size(); ++i) {
        if (edits[i].type != DiffLineType::Context) {
            change_indices.push_back(i);
        }
    }

    if (change_indices.empty()) {
        return fd;
    }

    // Build hunk ranges
    struct Range {
        size_t start;
        size_t end;
    };
    std::vector<Range> ranges;
    for (size_t idx : change_indices) {
        size_t h_start = (idx >= context_lines) ? (idx - context_lines) : 0;
        size_t h_end = std::min(edits.size(), idx + context_lines + 1);

        if (!ranges.empty() && h_start <= ranges.back().end) {
            ranges.back().end = std::max(ranges.back().end, h_end);
        } else {
            ranges.push_back({h_start, h_end});
        }
    }

    // Build DiffHunks with accurate old and new line numbers
    for (const auto& r : ranges) {
        DiffHunk hunk;
        size_t old_line = 1;
        size_t new_line = 1;

        for (size_t i = 0; i < r.start; ++i) {
            if (edits[i].type == DiffLineType::Context || edits[i].type == DiffLineType::Deletion) {
                old_line++;
            }
            if (edits[i].type == DiffLineType::Context || edits[i].type == DiffLineType::Addition) {
                new_line++;
            }
        }

        hunk.old_start = old_line;
        hunk.new_start = new_line;

        for (size_t i = r.start; i < r.end; ++i) {
            hunk.lines.push_back({edits[i].type, edits[i].text});
            if (edits[i].type == DiffLineType::Context || edits[i].type == DiffLineType::Deletion) {
                hunk.old_count++;
            }
            if (edits[i].type == DiffLineType::Context || edits[i].type == DiffLineType::Addition) {
                hunk.new_count++;
            }
        }

        fd.hunks.push_back(std::move(hunk));
    }

    return fd;
}

std::string DiffEngine::format_diff(const FileDiff& diff, bool color) {
    if (diff.empty()) {
        return "";
    }

    std::ostringstream ss;
    using namespace term;

    if (diff.is_binary) {
        std::string msg = "Binary files " + diff.old_path + " and " + diff.new_path + " differ\n";
        return color ? Terminal::yellow(msg) : msg;
    }

    std::string header_a = "--- a/" + diff.old_path + "\n";
    std::string header_b = "+++ b/" + diff.new_path + "\n";

    if (color) {
        ss << Terminal::bold(header_a);
        ss << Terminal::bold(header_b);
    } else {
        ss << header_a << header_b;
    }

    for (const auto& hunk : diff.hunks) {
        std::ostringstream hss;
        hss << "@@ -" << hunk.old_start << "," << hunk.old_count
            << " +" << hunk.new_start << "," << hunk.new_count << " @@\n";
        std::string hunk_hdr = hss.str();

        if (color) {
            ss << Terminal::violet(hunk_hdr);
        } else {
            ss << hunk_hdr;
        }

        for (const auto& line : hunk.lines) {
            if (line.type == DiffLineType::Addition) {
                std::string l = "+" + line.text + "\n";
                ss << (color ? Terminal::green(l) : l);
            } else if (line.type == DiffLineType::Deletion) {
                std::string l = "-" + line.text + "\n";
                ss << (color ? Terminal::red(l) : l);
            } else {
                std::string l = " " + line.text + "\n";
                ss << (color ? Terminal::dim(l) : l);
            }
        }
    }

    return ss.str();
}

std::string DiffEngine::format_stats(const std::vector<FileDiff>& diffs) {
    size_t files_changed = 0;
    size_t total_ins = 0;
    size_t total_del = 0;

    for (const auto& d : diffs) {
        if (!d.empty()) {
            files_changed++;
            total_ins += d.insertions;
            total_del += d.deletions;
        }
    }

    if (files_changed == 0) {
        return "";
    }

    std::ostringstream ss;
    ss << files_changed << " file" << (files_changed == 1 ? "" : "s") << " changed, "
       << total_ins << " insertion" << (total_ins == 1 ? "" : "s") << "(+), "
       << total_del << " deletion" << (total_del == 1 ? "" : "s") << "(-)";
    return ss.str();
}

} // namespace light_git
