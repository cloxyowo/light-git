#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_push(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    std::string remote_name = "origin";
    std::string branch_name;
    bool force = ctx.has_flag("f") || ctx.has_flag("force");

    if (!ctx.args.empty()) {
        remote_name = ctx.args[0];
        if (ctx.args.size() > 1) {
            branch_name = ctx.args[1];
        }
    }

    try {
        auto res = repo.push_remote(remote_name, branch_name, force);

        std::cout << Terminal::dim("To ") << Terminal::violet(remote_name) << "\n";
        for (const auto& r : res.results) {
            if (r.old_commit == r.new_commit) {
                std::cout << Terminal::green("Everything up-to-date") << "\n";
            } else if (r.is_new) {
                std::cout << " * [" << Terminal::green("new branch") << "]      "
                          << r.branch << " -> " << r.branch << "\n";
            } else {
                std::cout << "   " << Terminal::cyan(r.old_commit.to_short_hex())
                          << ".." << Terminal::cyan(r.new_commit.to_short_hex()) << "  "
                          << r.branch << " -> " << r.branch << "\n";
            }
        }
        if (res.objects_transferred > 0) {
            std::cout << Terminal::green("Pushed " + std::to_string(res.objects_transferred) + " objects successfully.") << "\n";
        }
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
