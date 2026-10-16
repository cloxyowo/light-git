#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_worktree(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    std::string subcmd = "list";
    if (!ctx.args.empty()) {
        subcmd = ctx.args[0];
    }

    try {
        if (subcmd == "list") {
            auto wts = repo.list_worktrees();
            for (const auto& wt : wts) {
                std::cout << wt.path.string() << "  "
                          << Terminal::cyan(wt.commit_id.to_short_hex()) << " "
                          << "[" << Terminal::violet(wt.branch) << "]";
                if (wt.is_main) {
                    std::cout << " (main worktree)";
                }
                if (wt.is_locked) {
                    std::cout << " (locked)";
                }
                std::cout << "\n";
            }
            return 0;
        }

        if (subcmd == "add") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit worktree add <path> [<branch>]");
                return 1;
            }
            std::filesystem::path wt_path = ctx.args[1];
            std::string branch = ctx.get_option("b", "");
            if (branch.empty() && ctx.args.size() > 2) {
                branch = ctx.args[2];
            }
            if (branch.empty()) {
                branch = wt_path.filename().string();
            }

            repo.add_worktree(wt_path, branch);
            std::cout << Terminal::green("Preparing worktree (checking out '")
                      << Terminal::bold_violet(branch)
                      << Terminal::green("') at ") << wt_path.string() << "\n";
            return 0;
        }

        if (subcmd == "remove") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit worktree remove <path> [--force]");
                return 1;
            }
            std::filesystem::path wt_path = ctx.args[1];
            bool force = ctx.has_flag("f") || ctx.has_flag("force");

            repo.remove_worktree(wt_path, force);
            std::cout << Terminal::violet("Removed worktree at '") << wt_path.string() << Terminal::violet("'") << "\n";
            return 0;
        }

        if (subcmd == "prune") {
            size_t count = repo.prune_worktrees();
            std::cout << Terminal::green("Pruned " + std::to_string(count) + " stale worktree(s).") << "\n";
            return 0;
        }

        Terminal::print_error("Unknown worktree subcommand: '" + subcmd + "'. Valid options: list, add, remove, prune");
        return 1;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
