#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_restore(const CommandContext& ctx) {
    using namespace term;

    if (ctx.args.empty()) {
        Terminal::print_error("you must specify path(s) to restore");
        return 1;
    }

    Repository repo = Repository::open(".");
    bool staged = ctx.has_flag("staged");

    std::string source_str = ctx.get_option("source", "");
    if (source_str.empty()) {
        source_str = ctx.get_option("s", "");
    }
    std::optional<ObjectId> source_id;
    if (!source_str.empty()) {
        source_id = repo.resolve_revision(source_str);
        if (!source_id) {
            Terminal::print_error("Failed to resolve source revision '" + source_str + "'");
            return 1;
        }
    }

    for (const auto& path : ctx.args) {
        repo.restore_file(path, staged, source_id);
    }

    return 0;
}

} // namespace commands
} // namespace light_git
