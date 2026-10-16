#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_gc(const CommandContext& ctx) {
    (void)ctx;
    using namespace term;

    Repository repo = Repository::open(".");

    std::cout << Terminal::dim("Running garbage collection...") << "\n";
    auto res = repo.garbage_collect();

    std::cout << Terminal::green(Terminal::sym_check() + " Garbage collection complete: ")
              << res.kept_objects << " reachable objects retained, "
              << res.removed_objects << " unreachable objects removed.\n";

    return 0;
}

} // namespace commands
} // namespace light_git
