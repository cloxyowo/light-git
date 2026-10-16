#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_reset(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");

    ResetMode mode = ResetMode::Mixed;
    if (ctx.has_flag("soft")) {
        mode = ResetMode::Soft;
    } else if (ctx.has_flag("hard")) {
        mode = ResetMode::Hard;
    }

    ObjectId target = repo.refs().resolve_head();

    if (!ctx.args.empty()) {
        std::string ref_str = ctx.args.front();
        if (ref_str == "HEAD") {
            target = repo.refs().resolve_head();
        } else if (ref_str.rfind("HEAD~", 0) == 0) {
            int steps = std::stoi(ref_str.substr(5));
            ObjectId cur = repo.refs().resolve_head();
            for (int i = 0; i < steps && cur; ++i) {
                RawObject robj = repo.objects().read_object(cur);
                Commit c = Commit::deserialize(robj.data);
                if (c.parents.empty()) break;
                cur = c.parents.front();
            }
            target = cur;
        } else {
            auto opt = repo.objects().resolve_prefix(ref_str);
            if (opt) {
                target = *opt;
            } else {
                auto bopt = repo.refs().get_branch_commit(ref_str);
                if (bopt) {
                    target = *bopt;
                } else {
                    Terminal::print_error("Failed to resolve commit '" + ref_str + "'");
                    return 1;
                }
            }
        }
    }

    if (!target) {
        Terminal::print_error("Cannot reset: no commit found to reset to.");
        return 1;
    }

    repo.reset(target, mode);

    std::string mode_str = (mode == ResetMode::Soft) ? "soft" :
                          (mode == ResetMode::Hard) ? "hard" : "mixed";

    std::cout << "HEAD is now at " << Terminal::bold_violet(target.to_short_hex())
              << " (" << mode_str << " reset)\n";

    return 0;
}

} // namespace commands
} // namespace light_git
