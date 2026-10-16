#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_tag(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    // Delete tag: -d / --delete
    if (ctx.has_flag("d") || ctx.has_flag("delete")) {
        std::string tag_name = ctx.get_option("d", "");
        if (tag_name.empty()) tag_name = ctx.get_option("delete", "");
        if (tag_name.empty() && !ctx.args.empty()) {
            tag_name = ctx.args[0];
        }

        if (tag_name.empty()) {
            Terminal::print_error("tag name required for deletion");
            return 1;
        }

        try {
            repo.delete_tag(tag_name);
            std::cout << Terminal::violet("Deleted tag '")
                      << tag_name
                      << Terminal::violet("'") << "\n";
            return 0;
        } catch (const std::exception& e) {
            Terminal::print_error(e.what());
            return 1;
        }
    }

    // List tags: no positional arguments and no -a
    if (ctx.args.empty() && !ctx.has_flag("a") && ctx.get_option("a", "").empty()) {
        auto tags = repo.list_tags();
        for (const auto& t : tags) {
            std::cout << Terminal::violet(t) << "\n";
        }
        return 0;
    }

    // Create tag
    std::string tag_name = ctx.get_option("a", "");
    std::string target_ref;
    std::string message = ctx.get_option("m", "");

    if (tag_name.empty() && !ctx.args.empty()) {
        tag_name = ctx.args[0];
        if (ctx.args.size() > 1) {
            target_ref = ctx.args[1];
        }
    }

    if (tag_name.empty()) {
        Terminal::print_error("tag name required");
        return 1;
    }

    try {
        ObjectId target_id = ObjectId::zero();
        if (!target_ref.empty()) {
            auto opt = repo.resolve_revision(target_ref);
            if (!opt) {
                Terminal::print_error("Failed to resolve target revision '" + target_ref + "'");
                return 1;
            }
            target_id = *opt;
        } else {
            target_id = repo.refs().resolve_head();
        }

        repo.create_tag(tag_name, target_id, message);
        std::cout << Terminal::bold_violet("Created tag '")
                  << tag_name
                  << Terminal::bold_violet("'")
                  << " at " << Terminal::cyan(target_id.to_short_hex()) << "\n";
        return 0;
    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
