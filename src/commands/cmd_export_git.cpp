#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include "light_git/filesystem.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_export_git(const CommandContext& ctx) {
    using namespace term;

    Repository repo = Repository::open(std::filesystem::current_path());

    std::string out_file = ctx.get_option("output", "");
    std::string target_dir = ctx.get_option("target", "");

    try {
        if (!out_file.empty()) {
            std::string fi_content = repo.export_git_fastimport();
            fs_util::atomic_write_file(out_file, fi_content);
            std::cout << Terminal::green("Exported Git fast-import stream to: ")
                      << Terminal::bold(out_file) << "\n";
            std::cout << Terminal::dim("You can import this stream into Git using: git init && git fast-import < ") << out_file << "\n";
            return 0;
        }

        std::filesystem::path target_path;
        if (!target_dir.empty()) {
            target_path = std::filesystem::absolute(target_dir);
        } else {
            target_path = repo.work_dir() / "export_git";
        }

        std::cout << Terminal::bold_violet("Exporting light-git history to Git format...") << "\n";
        repo.export_to_git(target_path);

        std::cout << Terminal::green("Export complete: ") << target_path.string() << "\n\n";
        std::cout << Terminal::bold("To push this repository to GitHub:") << "\n";
        std::cout << "  cd \"" << target_path.string() << "\"\n";
        std::cout << "  git remote add origin https://github.com/<your-username>/<your-repo>.git\n";
        std::cout << "  git push -u origin main\n";

        return 0;

    } catch (const std::exception& e) {
        Terminal::print_error(e.what());
        return 1;
    }
}

} // namespace commands
} // namespace light_git
