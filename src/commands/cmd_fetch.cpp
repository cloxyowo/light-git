#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_fetch(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    std::string remote_name = "origin";
    std::string branch_name;

    if (!ctx.args.empty()) {
        remote_name = ctx.args[0];
        if (ctx.args.size() > 1) {
            branch_name = ctx.args[1];
        }
    }

    try {
        auto res = repo.fetch_remote(remote_name, branch_name);

        if (res.updated_branches.empty()) {
            std::cout << Terminal::dim("Already up to date with remote '" + remote_name + "'.") << "\n";
            return 0;
        }

        std::cout << Terminal::dim("From ") << Terminal::violet(remote_name) << "\n";
        for (const auto& b : res.updated_branches) {
            if (b.is_new) {
                std::cout << " * [" << Terminal::green("new branch") << "]      "
                          << b.branch << " -> " << remote_name << "/" << b.branch << "\n";
            } else {
                std::cout << "   " << Terminal::cyan(b.old_commit.to_short_hex())
                          << ".." << Terminal::cyan(b.new_commit.to_short_hex()) << "  "
                          << b.branch << " -> " << remote_name << "/" << b.branch << "\n";
            }
        }
        std::cout << Terminal::green("Transferred " + std::to_string(res.objects_transferred) + " objects.") << "\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
