#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

static size_t parse_stash_index(const std::string& arg) {
    std::string num_str = arg;
    if (arg.rfind("stash@{", 0) == 0 && arg.back() == '}') {
        num_str = arg.substr(7, arg.size() - 8);
    }
    try {
        size_t idx = 0;
        unsigned long val = std::stoul(num_str, &idx);
        if (idx != num_str.size()) {
            throw std::runtime_error("Invalid stash reference: '" + arg + "'");
        }
        return static_cast<size_t>(val);
    } catch (const std::runtime_error&) {
        throw;
    } catch (...) {
        throw std::runtime_error("Invalid stash reference: '" + arg + "'");
    }
}

int cmd_stash(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    std::string subcmd = "push";
    if (!ctx.args.empty()) {
        std::string first = ctx.args[0];
        if (first == "list" || first == "pop" || first == "apply" || first == "drop" || first == "clear" || first == "push") {
            subcmd = first;
        }
    }

    try {
        if (subcmd == "list") {
            auto stashes = repo.list_stashes();
            if (stashes.empty()) {
                return 0;
            }
            for (const auto& s : stashes) {
                std::cout << Terminal::violet("stash@{" + std::to_string(s.index) + "}: ")
                          << Terminal::bold(s.message) << "\n";
            }
            return 0;
        }

        if (subcmd == "pop" || subcmd == "apply") {
            size_t idx = 0;
            if (ctx.args.size() > 1) {
                idx = parse_stash_index(ctx.args[1]);
            }
            if (subcmd == "pop") {
                repo.stash_pop(idx);
                std::cout << Terminal::green("Applied and dropped stash@{" + std::to_string(idx) + "}") << "\n";
            } else {
                repo.stash_apply(idx);
                std::cout << Terminal::green("Applied stash@{" + std::to_string(idx) + "}") << "\n";
            }
            return 0;
        }

        if (subcmd == "drop") {
            size_t idx = 0;
            if (ctx.args.size() > 1) {
                idx = parse_stash_index(ctx.args[1]);
            }
            repo.stash_drop(idx);
            std::cout << Terminal::violet("Dropped stash@{" + std::to_string(idx) + "}") << "\n";
            return 0;
        }

        if (subcmd == "clear") {
            repo.stash_clear();
            std::cout << Terminal::violet("Cleared all stashed states.") << "\n";
            return 0;
        }

        // Default: push
        std::string message = ctx.get_option("m", "");
        if (message.empty() && ctx.args.size() > 1 && subcmd == "push") {
            message = ctx.args[1];
        }

        ObjectId stash_id = repo.stash_push(message);
        std::cout << Terminal::bold_violet("Saved working directory and index state ")
                  << Terminal::cyan(stash_id.to_short_hex()) << "\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
