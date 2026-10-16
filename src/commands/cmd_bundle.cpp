#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_bundle(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty()) {
        Terminal::print_error("usage: lgit bundle <create|verify|unbundle|list-heads> <file.lgitpack> [<rev>...]");
        return 1;
    }

    std::string subcmd = ctx.args[0];

    try {
        if (subcmd == "create") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit bundle create <file.lgitpack> [<rev>...]");
                return 1;
            }
            std::filesystem::path bundle_file = ctx.args[1];
            std::vector<std::string> revs;
            for (size_t i = 2; i < ctx.args.size(); ++i) {
                revs.push_back(ctx.args[i]);
            }

            Repository repo = Repository::open(std::filesystem::current_path());
            repo.create_bundle(bundle_file, revs);
            std::cout << Terminal::green("Created bundle: ") << bundle_file.string() << "\n";
            return 0;
        }

        if (subcmd == "verify" || subcmd == "list-heads") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit bundle " + subcmd + " <file.lgitpack>");
                return 1;
            }
            std::filesystem::path bundle_file = ctx.args[1];
            auto info = Repository::verify_bundle(bundle_file);

            std::cout << Terminal::green("Bundle checksum and header verified: ") << bundle_file.string() << "\n";
            std::cout << "The bundle contains " << info.object_count << " objects and " << info.references.size() << " ref(s):\n";
            for (const auto& [rname, rid] : info.references) {
                std::cout << "  " << Terminal::cyan(rid.to_short_hex()) << " " << Terminal::violet(rname) << "\n";
            }
            return 0;
        }

        if (subcmd == "unbundle") {
            if (ctx.args.size() < 2) {
                Terminal::print_error("usage: lgit bundle unbundle <file.lgitpack>");
                return 1;
            }
            std::filesystem::path bundle_file = ctx.args[1];
            Repository repo = Repository::open(std::filesystem::current_path());
            auto info = repo.unbundle(bundle_file);

            std::cout << Terminal::green("Unbundled ") << info.object_count << " objects from " << bundle_file.string() << "\n";
            for (const auto& [rname, rid] : info.references) {
                std::cout << " * [" << Terminal::green("ref") << "] " << rname << " -> " << Terminal::cyan(rid.to_short_hex()) << "\n";
            }
            return 0;
        }

        Terminal::print_error("Unknown bundle subcommand: '" + subcmd + "'. Valid options: create, verify, unbundle, list-heads");
        return 1;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
