#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_pull(const CommandContext& ctx) {
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
        auto pull_res = repo.pull_remote(remote_name, branch_name);

        if (!pull_res.fetch_res.updated_branches.empty()) {
            std::cout << Terminal::dim("From ") << Terminal::violet(remote_name) << "\n";
            for (const auto& b : pull_res.fetch_res.updated_branches) {
                std::cout << "   " << Terminal::cyan(b.old_commit.to_short_hex())
                          << ".." << Terminal::cyan(b.new_commit.to_short_hex()) << "  "
                          << b.branch << " -> " << remote_name << "/" << b.branch << "\n";
            }
        }

        const auto& m = pull_res.merge_res;
        if (m.already_up_to_date) {
            std::cout << Terminal::green("Already up to date.") << "\n";
            return 0;
        }

        if (m.is_fast_forward) {
            std::cout << Terminal::bold_violet("Updating ")
                      << Terminal::cyan(m.commit_id.to_short_hex()) << "\n";
            std::cout << Terminal::green("Fast-forward") << "\n";
            return 0;
        }

        if (m.has_conflicts) {
            for (const auto& f : m.conflict_files) {
                std::cout << Terminal::red("CONFLICT (content): Merge conflict in ")
                          << Terminal::bold(f) << "\n";
            }
            std::cout << Terminal::yellow(m.message) << "\n";
            return 1;
        }

        std::cout << Terminal::green("Merge made by the 'three-way' strategy.") << "\n";
        std::cout << "[" << Terminal::violet(repo.refs().get_current_branch()) << " "
                  << Terminal::cyan(m.commit_id.to_short_hex()) << "] "
                  << "Merge remote tracking branch '" << remote_name << "/" << (branch_name.empty() ? repo.refs().get_current_branch() : branch_name) << "'\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
