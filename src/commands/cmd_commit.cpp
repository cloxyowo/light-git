#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include "light_git/diff.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_commit(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");

    std::string message = ctx.get_option("m", "");
    bool amend = ctx.has_flag("amend");

    if (message.empty()) {
        Terminal::print_error("Aborting commit due to empty commit message.");
        std::cout << Terminal::dim("Please provide a commit message with 'lgit commit -m \"Your message\"'") << "\n";
        return 1;
    }

    RepoStatus status = repo.compute_status();
    size_t staged_count = status.staged_new.size() + status.staged_modified.size() + status.staged_deleted.size();

    if (staged_count == 0 && !amend) {
        Terminal::print_error("no changes added to commit (use 'lgit add' to track or stage changes)");
        return 1;
    }

    // Get current staged diffs to compute stats
    auto staged_diffs = repo.diff_index_vs_head();
    std::string diff_stats = DiffEngine::format_stats(staged_diffs);

    ObjectId commit_id = repo.create_commit(message, amend);

    std::string branch = repo.refs().is_head_detached() ? "detached" : repo.refs().get_current_branch();
    if (branch.empty()) branch = "main";

    // First line of message
    std::string first_line = message;
    size_t nl = first_line.find('\n');
    if (nl != std::string::npos) {
        first_line = first_line.substr(0, nl);
    }

    std::cout << "\n[" << Terminal::bold_violet(branch) << " "
              << Terminal::violet(commit_id.to_short_hex()) << "] "
              << first_line << "\n\n";

    if (!diff_stats.empty()) {
        std::cout << " " << diff_stats << "\n\n";
    }

    std::cout << Terminal::green("commit created successfully") << "\n\n";

    return 0;
}

} // namespace commands
} // namespace light_git
