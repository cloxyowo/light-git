#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_init(const CommandContext& ctx) {
    using namespace term;

    std::filesystem::path target_path = ".";
    if (!ctx.args.empty()) {
        target_path = ctx.args.front();
    }

    bool bare = ctx.has_flag("bare");

    // Check if repository already exists
    std::filesystem::path check_dir = bare ? target_path : (target_path / ".light-git");
    if (std::filesystem::exists(check_dir)) {
        Terminal::print_warning("Reinitializing existing light-git repository in " +
                                std::filesystem::absolute(check_dir).generic_string());
    }

    Repository repo = Repository::init(target_path, bare, "main");

    std::cout << Terminal::bold_violet("Initialized empty light-git repository")
              << " in " << Terminal::dim(std::filesystem::absolute(repo.git_dir()).generic_string()) << "\n";
    std::cout << Terminal::dim("default branch: ") << Terminal::violet("main") << "\n";

    return 0;
}

} // namespace commands
} // namespace light_git
