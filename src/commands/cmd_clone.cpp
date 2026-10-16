#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_clone(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty()) {
        Terminal::print_error("You must specify a repository to clone. Usage: lgit clone <source> [<directory>]");
        return 1;
    }

    std::string source = ctx.args[0];
    std::filesystem::path target_dir;

    if (ctx.args.size() > 1) {
        target_dir = ctx.args[1];
    } else {
        // Derive directory name from source
        std::filesystem::path sp(source);
        std::string stem = sp.stem().string();
        if (stem.empty() || stem == ".") stem = "repo";
        target_dir = stem;
    }

    std::string branch = ctx.get_option("b", "");
    if (branch.empty()) {
        branch = ctx.get_option("branch", "");
    }

    try {
        std::cout << Terminal::dim("Cloning into '") << target_dir.string() << Terminal::dim("'...") << "\n";
        Repository repo = Repository::clone_repository(source, target_dir, branch);

        std::string cur_branch = repo.refs().get_current_branch();
        ObjectId head_id = repo.refs().resolve_head();

        std::cout << Terminal::green("Clone successful: ")
                  << "Checked out branch '" << Terminal::bold_violet(cur_branch.empty() ? "main" : cur_branch) << "' at "
                  << Terminal::cyan(head_id.to_short_hex()) << "\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
