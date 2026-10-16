#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_cherry_pick(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    if (ctx.args.empty()) {
        Terminal::print_error("commit revision required (e.g. 'lgit cherry-pick <commit>')");
        return 1;
    }

    std::string commit_ref = ctx.args[0];

    try {
        auto opt = repo.resolve_revision(commit_ref);
        if (!opt) {
            Terminal::print_error("Failed to resolve commit '" + commit_ref + "'");
            return 1;
        }

        ObjectId new_commit = repo.cherry_pick(*opt);
        std::string cur_branch = repo.refs().get_current_branch();
        if (cur_branch.empty()) cur_branch = "HEAD";

        RawObject obj = repo.objects().read_object(new_commit);
        Commit c = Commit::deserialize(obj.data);
        std::string summary = c.message;
        size_t nl = summary.find('\n');
        if (nl != std::string::npos) summary = summary.substr(0, nl);

        std::cout << "[" << Terminal::violet(cur_branch) << " "
                  << Terminal::cyan(new_commit.to_short_hex()) << "] "
                  << summary << "\n";
        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
