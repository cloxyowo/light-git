#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_branch(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");

    bool is_delete = ctx.has_flag("d") || ctx.has_flag("D") || ctx.has_flag("delete");
    bool force_delete = ctx.has_flag("D");
    std::string del_branch = ctx.get_option("d", "");
    if (del_branch.empty()) {
        del_branch = ctx.get_option("D", "");
        if (!del_branch.empty()) force_delete = true;
    }
    if (del_branch.empty()) {
        del_branch = ctx.get_option("delete", "");
    }
    if (del_branch.empty() && is_delete) {
        if (!ctx.args.empty()) {
            del_branch = ctx.args.front();
        } else {
            Terminal::print_error("Missing branch name to delete.");
            return 1;
        }
    }

    if (!del_branch.empty()) {
        repo.delete_branch(del_branch, force_delete);
        std::cout << "Deleted branch " << Terminal::bold_violet(del_branch) << ".\n";
        return 0;
    }

    if (ctx.args.empty()) {
        // List branches
        auto branches = repo.refs().list_branches();
        std::string cur_branch = repo.refs().get_current_branch();

        if (branches.empty()) {
            std::cout << Terminal::dim("No branches found.") << "\n";
            return 0;
        }

        for (const auto& b : branches) {
            if (b == cur_branch && !repo.refs().is_head_detached()) {
                std::cout << Terminal::green("* ") << Terminal::bold_violet(b) << "\n";
            } else {
                std::cout << "  " << b << "\n";
            }
        }
        return 0;
    }

    // Create branch
    std::string new_branch = ctx.args.front();
    repo.create_branch(new_branch);
    std::cout << "Created branch " << Terminal::bold_violet(new_branch) << "\n";

    return 0;
}

} // namespace commands
} // namespace light_git
