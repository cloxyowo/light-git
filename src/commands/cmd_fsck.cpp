#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_fsck(const CommandContext& ctx) {
    (void)ctx;
    using namespace term;

    Repository repo = Repository::open(".");

    std::cout << Terminal::dim("Verifying object store integrity...") << "\n";
    auto issues = repo.verify_integrity();

    if (issues.empty()) {
        std::cout << Terminal::green(Terminal::sym_check() + " Repository integrity check passed.")
                  << " All objects, trees, and references verified.\n";
        return 0;
    }

    std::cout << Terminal::red(Terminal::sym_cross() + " Integrity verification found issues:\n");
    for (const auto& issue : issues) {
        if (issue.severity == Repository::FsckIssue::Severity::Error) {
            std::cout << "  " << Terminal::red("error: ") << issue.message << "\n";
        } else {
            std::cout << "  " << Terminal::yellow("warning: ") << issue.message << "\n";
        }
    }

    return 1;
}

} // namespace commands
} // namespace light_git
