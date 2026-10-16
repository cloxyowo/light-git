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
    std::vector<FileDiff> diffs = staged ? repo.diff_index_vs_head() : repo.diff_working_vs_index();

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
