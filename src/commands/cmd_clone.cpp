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
        // Derive directory name from source (supports URLs, paths, and .git / .lgitpack extensions)
        std::string s = source;
        while (!s.empty() && (s.back() == '/' || s.back() == '\\' || s.back() == ' ')) {
            s.pop_back();
        }
        if (s.size() > 4 && s.substr(s.size() - 4) == ".git") {
            s.erase(s.size() - 4);
        }
        if (s.size() > 9 && s.substr(s.size() - 9) == ".lgitpack") {
            s.erase(s.size() - 9);
        }
        size_t slash = s.find_last_of("/\\:");
        std::string name = (slash != std::string::npos) ? s.substr(slash + 1) : s;
        if (name.empty() || name == ".") name = "repo";
        target_dir = name;
    }

    std::string branch = ctx.get_option("b", "");
    if (branch.empty()) {
        branch = ctx.get_option("branch", "");
    }
    std::string depth_str = ctx.get_option("depth", "");
    int depth = 0;
    if (!depth_str.empty()) {
        try { depth = std::stoi(depth_str); } catch (...) {}
    }

    try {
        std::cout << Terminal::dim("Cloning into '") << target_dir.string() << Terminal::dim("'...") << "\n";
        Repository repo = Repository::clone_repository(source, target_dir, branch, depth);

        std::string cur_branch = repo.refs().get_current_branch();
        ObjectId head_id = repo.refs().resolve_head();

        std::cout << Terminal::green("Clone successful: ")
                  << "Checked out branch '" << Terminal::bold_violet(cur_branch.empty() ? "main" : cur_branch) << "'";
        if (head_id) {
            std::cout << " at " << Terminal::cyan(head_id.to_short_hex());
        }
        std::cout << "\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
