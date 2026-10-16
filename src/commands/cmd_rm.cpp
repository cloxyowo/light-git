#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_rm(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty()) {
        Terminal::print_error("No files specified to remove.");
        return 1;
    }

    Repository repo = Repository::open(".");
    bool cached = ctx.has_flag("cached");

    for (const auto& path : ctx.args) {
        repo.remove_path(path, cached);
        std::cout << "rm '" << path << "'\n";
    }

    return 0;
}

} // namespace commands
} // namespace light_git
