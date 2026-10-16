#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include "light_git/diff.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_diff(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");

    bool staged = ctx.has_flag("staged") || ctx.has_flag("cached");
    std::vector<FileDiff> diffs;
    std::string path_filter;

    if (ctx.args.empty()) {
        diffs = staged ? repo.diff_index_vs_head() : repo.diff_working_vs_index();
    } else if (ctx.args.size() == 1) {
        auto rev = repo.resolve_revision(ctx.args[0]);
        if (rev) {
            diffs = staged ? repo.diff_commits(*rev, repo.refs().resolve_head()) : repo.diff_working_vs_commit(*rev);
        } else {
            // Treat as path filter
            path_filter = ctx.args[0];
            diffs = staged ? repo.diff_index_vs_head() : repo.diff_working_vs_index();
        }
    } else if (ctx.args.size() == 2) {
        auto rev1 = repo.resolve_revision(ctx.args[0]);
        auto rev2 = repo.resolve_revision(ctx.args[1]);
        if (rev1 && rev2) {
            diffs = repo.diff_commits(*rev1, *rev2);
        } else if (rev1 && !rev2) {
            path_filter = ctx.args[1];
            diffs = repo.diff_working_vs_commit(*rev1);
        } else {
            Terminal::print_error("Failed to resolve revisions for diff");
            return 1;
        }
    } else {
        Terminal::print_error("Too many arguments for diff");
        return 1;
    }

    if (!path_filter.empty()) {
        std::vector<FileDiff> filtered;
        for (auto& d : diffs) {
            if (d.new_path == path_filter || d.old_path == path_filter) {
                filtered.push_back(std::move(d));
            }
        }
        diffs = std::move(filtered);
    }

    if (diffs.empty()) {
        return 0;
    }

    bool color = Terminal::is_color_enabled();
    for (const auto& d : diffs) {
        std::cout << DiffEngine::format_diff(d, color);
    }

    return 0;
}

} // namespace commands
} // namespace light_git
