#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_add(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty() && !ctx.has_flag("A") && !ctx.has_flag("all")) {
        Terminal::print_error("Nothing specified, nothing added.");
        std::cout << Terminal::dim("Maybe you wanted to say 'lgit add .' or 'lgit add -A' ?") << "\n";
        return 1;
    }

    Repository repo = Repository::open(".");

    if (ctx.has_flag("A") || ctx.has_flag("all")) {
        repo.stage_all();
        return 0;
    }

    for (const auto& arg : ctx.args) {
        if (arg == "." || arg == "-A") {
            repo.stage_all();
        } else {
            repo.stage_path(arg);
        }
    }

    return 0;
}

} // namespace commands
} // namespace light_git
