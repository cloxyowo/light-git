#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_remote(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    bool verbose = ctx.has_flag("v") || ctx.has_flag("verbose");

    if (ctx.args.empty()) {
        auto remotes = repo.list_remotes();
        for (const auto& r : remotes) {
            if (verbose) {
                std::cout << Terminal::violet(r.name) << "\t" << r.url << " (fetch)\n";
                std::cout << Terminal::violet(r.name) << "\t" << r.url << " (push)\n";
            } else {
                std::cout << Terminal::violet(r.name) << "\n";
            }
        }
        return 0;
    }

    std::string subcmd = ctx.args[0];

    try {
        if (subcmd == "add") {
            if (ctx.args.size() < 3) {
                Terminal::print_error("usage: lgit remote add <name> <url>");
                return 1;
            }
            std::string name = ctx.args[1];
            std::string url = ctx.args[2];
            repo.add_remote(name, url);
            std::cout << Terminal::green("Added remote '")
                      << Terminal::bold_violet(name)
                      << Terminal::green("' -> ") << url << "\n";
            return 0;
        }

        if (subcmd == "remove" || subcmd == "rm") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit remote remove <name>");
                return 1;
            }
            std::string name = ctx.args[1];
            repo.remove_remote(name);
            std::cout << Terminal::violet("Removed remote '") << name << Terminal::violet("'") << "\n";
            return 0;
        }

        if (subcmd == "get-url") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit remote get-url <name>");
                return 1;
            }
            std::string name = ctx.args[1];
            auto url = repo.get_remote_url(name);
            if (!url) {
                Terminal::print_error("No such remote '" + name + "'");
                return 1;
            }
            std::cout << *url << "\n";
            return 0;
        }

        if (subcmd == "set-url") {
            if (ctx.args.size() < 3) {
                Terminal::print_error("usage: lgit remote set-url <name> <newurl>");
                return 1;
            }
            std::string name = ctx.args[1];
            std::string newurl = ctx.args[2];
            repo.set_remote_url(name, newurl);
            std::cout << Terminal::green("Updated remote '")
                      << Terminal::bold_violet(name)
                      << Terminal::green("' -> ") << newurl << "\n";
            return 0;
        }

        Terminal::print_error("Unknown remote subcommand: '" + subcmd + "'. Valid options: add, remove, get-url, set-url");
        return 1;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
