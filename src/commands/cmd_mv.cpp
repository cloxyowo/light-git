#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_mv(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.size() < 2) {
        Terminal::print_error("Destination path missing. Usage: lgit mv <source> <destination>");
        return 1;
    }

    Repository repo = Repository::open(".");
    std::string src = ctx.args[0];
    std::string dest = ctx.args[1];

    repo.move_path(src, dest);
    std::cout << "Renamed '" << Terminal::bold_violet(src) << "' "
              << Terminal::sym_arrow() << " '" << Terminal::bold_violet(dest) << "'\n";

    return 0;
}

} // namespace commands
} // namespace light_git
