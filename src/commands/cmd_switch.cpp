#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_switch(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");

    bool create_flag = ctx.has_flag("c") || ctx.has_flag("create");
    std::string new_branch = ctx.get_option("c", "");
    if (new_branch.empty()) {
        new_branch = ctx.get_option("create", "");
    }
    if (new_branch.empty() && create_flag) {
        if (!ctx.args.empty()) {
            new_branch = ctx.args.front();
        } else {
            Terminal::print_error("Missing branch name for -c / --create.");
            return 1;
        }
    }

    if (!new_branch.empty()) {
        repo.switch_branch(new_branch, true);
        std::cout << "Switched to a new branch '" << Terminal::bold_violet(new_branch) << "'\n";
        return 0;
    }

    if (ctx.args.empty()) {
        Terminal::print_error("Missing branch name to switch to.");
        return 1;
    }

    std::string target_branch = ctx.args.front();
    repo.switch_branch(target_branch, false);
    std::cout << "Switched to branch '" << Terminal::bold_violet(target_branch) << "'\n";

    return 0;
}

} // namespace commands
} // namespace light_git
