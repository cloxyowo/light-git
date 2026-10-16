#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_merge(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    if (ctx.args.empty()) {
        Terminal::print_error("branch or revision to merge required (e.g. 'lgit merge feature')");
        return 1;
    }

    std::string target = ctx.args[0];

    try {
        auto result = repo.merge(target);

        if (result.already_up_to_date) {
            std::cout << Terminal::green("Already up to date.") << "\n";
            return 0;
        }

        if (result.is_fast_forward) {
            std::cout << Terminal::bold_violet("Updating ")
                      << Terminal::cyan(result.commit_id.to_short_hex()) << "\n";
            std::cout << Terminal::green("Fast-forward") << "\n";
            return 0;
        }

        if (result.has_conflicts) {
            for (const auto& f : result.conflict_files) {
                std::cout << Terminal::red("CONFLICT (content): Merge conflict in ")
                          << Terminal::bold(f) << "\n";
            }
            std::cout << Terminal::yellow(result.message) << "\n";
            return 1;
        }

        // Clean 3-way merge
        std::cout << Terminal::green("Merge made by the 'three-way' strategy.") << "\n";
        std::string cur_branch = repo.refs().get_current_branch();
        if (cur_branch.empty()) cur_branch = "HEAD";
        std::cout << "[" << Terminal::violet(cur_branch) << " "
                  << Terminal::cyan(result.commit_id.to_short_hex()) << "] "
                  << "Merge branch '" << target << "'\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
