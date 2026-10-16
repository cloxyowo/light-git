#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>
#include <sstream>

namespace light_git {
namespace commands {

int cmd_status(const CommandContext& ctx) {
    (void)ctx;
    using namespace term;

    Repository repo = Repository::open(".");
    RepoStatus status = repo.compute_status();

    // Header banner
    Terminal::print_banner("0.1.0");

    // Branch line
    std::string branch_display = status.is_detached ?
        Terminal::yellow("detached HEAD at " + status.head_commit.to_short_hex()) :
        Terminal::bold_violet(status.branch_name.empty() ? "main" : status.branch_name);

    Terminal::print_key_value("branch", branch_display);

    // Summary count
    size_t staged_count = status.staged_new.size() + status.staged_modified.size() + status.staged_deleted.size();
    size_t unstaged_count = status.unstaged_modified.size() + status.unstaged_deleted.size();
    size_t untracked_count = status.untracked.size();

    if (status.is_clean()) {
        Terminal::print_key_value("status", Terminal::green("working tree clean"));
        std::cout << "\n";
        return 0;
    }

    std::vector<std::string> parts;
    if (staged_count > 0) parts.push_back(std::to_string(staged_count) + " staged");
    if (unstaged_count > 0) parts.push_back(std::to_string(unstaged_count) + " unstaged");
    if (untracked_count > 0) parts.push_back(std::to_string(untracked_count) + " untracked");

    std::ostringstream ss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << parts[i];
    }
    Terminal::print_key_value("status", ss.str());
    std::cout << "\n";

    // 1. Changes to be committed (staged)
    if (staged_count > 0) {
        std::cout << Terminal::bold("Changes to be committed:\n");
        std::cout << Terminal::dim("  (use 'lgit restore --staged <file>...' to unstage)\n");
        for (const auto& path : status.staged_new) {
            std::cout << "  " << Terminal::green("new file:   " + path) << "\n";
        }
        for (const auto& path : status.staged_modified) {
            std::cout << "  " << Terminal::green("modified:   " + path) << "\n";
        }
        for (const auto& path : status.staged_deleted) {
            std::cout << "  " << Terminal::green("deleted:    " + path) << "\n";
        }
        std::cout << "\n";
    }

    // 2. Changes not staged for commit
    if (unstaged_count > 0) {
        std::cout << Terminal::bold("Changes not staged for commit:\n");
        std::cout << Terminal::dim("  (use 'lgit add <file>...' to update what will be committed)\n");
        std::cout << Terminal::dim("  (use 'lgit restore <file>...' to discard changes in working directory)\n");
        for (const auto& path : status.unstaged_modified) {
            std::cout << "  " << Terminal::red("modified:   " + path) << "\n";
        }
        for (const auto& path : status.unstaged_deleted) {
            std::cout << "  " << Terminal::red("deleted:    " + path) << "\n";
        }
        std::cout << "\n";
    }

    // 3. Untracked files
    if (untracked_count > 0) {
        std::cout << Terminal::bold("Untracked files:\n");
        std::cout << Terminal::dim("  (use 'lgit add <file>...' to include in what will be committed)\n");
        for (const auto& path : status.untracked) {
            std::cout << "  " << Terminal::yellow("?  " + path) << "\n";
        }
        std::cout << "\n";
    }

    if (unstaged_count > 0) {
        Terminal::print_tip("run 'lgit diff' to inspect your unstaged changes");
    } else if (staged_count > 0) {
        Terminal::print_tip("run 'lgit commit -m \"...\"' to commit staged changes");
    }

    return 0;
}

} // namespace commands
} // namespace light_git
