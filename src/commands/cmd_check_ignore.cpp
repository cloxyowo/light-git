#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_check_ignore(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty()) {
        Terminal::print_error("Missing pathspec. Usage: lgit check-ignore <path>...");
        return 1;
    }

    Repository repo = Repository::open(".");
    bool any_matched = false;

    for (const auto& path : ctx.args) {
        bool is_dir = std::filesystem::is_directory(repo.work_dir() / path);
        auto res = repo.ignores().check(path, is_dir);
        if (res.ignored) {
            any_matched = true;
            std::cout << Terminal::dim(res.source_file + ":" + std::to_string(res.line_number) + ":")
                      << Terminal::violet(res.matched_pattern) << "\t"
                      << path << "\n";
        }
    }

    return any_matched ? 0 : 1;
}

} // namespace commands
} // namespace light_git
