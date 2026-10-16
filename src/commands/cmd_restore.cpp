#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_restore(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty()) {
        Terminal::print_error("you must specify path(s) to restore");
        return 1;
    }

    Repository repo = Repository::open(".");
    bool staged = ctx.has_flag("staged");

    for (const auto& path : ctx.args) {
        repo.restore_file(path, staged);
    }

    return 0;
}

} // namespace commands
} // namespace light_git
