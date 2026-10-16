#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>

namespace light_git {
namespace commands {

int cmd_log(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(".");
    ObjectId cur = repo.refs().resolve_head();

    if (!cur) {
        std::cout << Terminal::dim("No commits yet on this repository.") << "\n";
        return 0;
    }

    bool oneline = ctx.has_flag("oneline");
    int max_count = -1;
    std::string n_opt = ctx.get_option("n", "");
    if (!n_opt.empty()) {
        try {
            max_count = std::stoi(n_opt);
        } catch (...) {}
    }

    int count = 0;
    std::string current_branch = repo.refs().get_current_branch();

    while (cur && (max_count < 0 || count < max_count)) {
        RawObject obj = repo.objects().read_object(cur);
        if (obj.type != ObjectType::Commit) break;

        Commit commit = Commit::deserialize(obj.data);
        std::string first_line = commit.message;
        size_t nl = first_line.find('\n');
        if (nl != std::string::npos) {
            first_line = first_line.substr(0, nl);
        }

        std::string ref_annotation;
        if (count == 0) {
            if (repo.refs().is_head_detached()) {
                ref_annotation = " (" + Terminal::cyan("HEAD") + ")";
            } else {
                ref_annotation = " (" + Terminal::cyan("HEAD -> ") + Terminal::green(current_branch) + ")";
            }
        }

        if (oneline) {
            std::cout << Terminal::violet("* ") << Terminal::bold_violet(cur.to_short_hex())
                      << ref_annotation << " " << first_line << "\n";
        } else {
            std::time_t ts = static_cast<std::time_t>(commit.author.timestamp);
            std::tm tm_buf{};
#if defined(_WIN32)
            localtime_s(&tm_buf, &ts);
#else
            localtime_r(&ts, &tm_buf);
#endif
            char time_str[64];
            std::strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", &tm_buf);

            std::cout << Terminal::violet("* ") << Terminal::bold("commit ") << Terminal::bold_violet(cur.to_hex())
                      << ref_annotation << "\n";
            std::cout << Terminal::violet("| ") << "Author: " << commit.author.name << " <" << commit.author.email << ">\n";
            std::cout << Terminal::violet("| ") << "Date:   " << time_str << " " << commit.author.timezone << "\n";
            std::cout << Terminal::violet("|\n");

            // Indent message lines
            std::istringstream mstream(commit.message);
            std::string mline;
            while (std::getline(mstream, mline)) {
                std::cout << Terminal::violet("| ") << "    " << mline << "\n";
            }
            std::cout << "\n";
        }

        count++;
        if (commit.parents.empty()) {
            break;
        }
        cur = commit.parents.front();
    }

    return 0;
}

} // namespace commands
} // namespace light_git
