#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include "light_git/diff.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>

namespace light_git {
namespace commands {

int cmd_show(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");

    ObjectId target = repo.refs().resolve_head();
    if (!ctx.args.empty()) {
        std::string ref_str = ctx.args.front();
        auto opt = repo.resolve_revision(ref_str);
        if (opt) {
            target = *opt;
        } else {
            Terminal::print_error("Failed to resolve revision '" + ref_str + "'");
            return 1;
        }
    }

    if (!target) {
        Terminal::print_error("No commit found to show.");
        return 1;
    }

    RawObject obj = repo.objects().read_object(target);
    if (obj.type != ObjectType::Commit) {
        Terminal::print_error("Object " + target.to_short_hex() + " is not a commit");
        return 1;
    }

    Commit commit = Commit::deserialize(obj.data);

    std::time_t ts = static_cast<std::time_t>(commit.author.timestamp);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &ts);
#else
    localtime_r(&ts, &tm_buf);
#endif
    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", &tm_buf);

    std::cout << Terminal::bold("commit ") << Terminal::bold_violet(target.to_hex()) << "\n";
    std::cout << "Author: " << commit.author.name << " <" << commit.author.email << ">\n";
    std::cout << "Date:   " << time_str << " " << commit.author.timezone << "\n\n";

    std::istringstream ss(commit.message);
    std::string line;
    while (std::getline(ss, line)) {
        std::cout << "    " << line << "\n";
    }
    std::cout << "\n";

    auto diffs = repo.diff_commit(target);
    bool color = Terminal::is_color_enabled();
    for (const auto& d : diffs) {
        std::cout << DiffEngine::format_diff(d, color);
    }

    return 0;
}

} // namespace commands
} // namespace light_git
