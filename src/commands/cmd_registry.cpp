#include "commands.hpp"
#include "light_git/terminal.hpp"
#include "light_git/types.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_version(const CommandContext& ctx) {
    (void)ctx;
    term::Terminal::print_banner(std::string(LIGHT_GIT_VERSION));
    std::cout << "built with C++17 for high-performance minimal footprint\n";
#if defined(_WIN32)
    std::cout << "platform: Windows (MSVC)\n";
#elif defined(__linux__)
    std::cout << "platform: Linux (GCC/Clang)\n";
#elif defined(__APPLE__)
    std::cout << "platform: macOS (Clang)\n";
#else
    std::cout << "platform: Native\n";
#endif
    return 0;
}

int cmd_future_stub(const CommandContext& ctx) {
    using namespace term;
    std::cout << Terminal::bold_violet("light-git: ")
              << "'" << ctx.command_name << "' is part of the future light-git roadmap.\n\n";
    std::cout << "This command is not yet enabled in v0.1.0 to preserve data safety guarantees.\n";
    std::cout << Terminal::dim("See 'docs/COMMANDS.md' or project documentation for planned architecture.") << "\n";
    return 0;
}

void register_all_commands(CliDispatcher& d) {
    // Core essential commands
    d.register_command({
        "init",
        "Initialize a new, empty light-git repository",
        "lgit init [<directory>] [--bare]",
        cmd_init,
        true
    });

    d.register_command({
        "status",
        "Show working tree and staging area status",
        "lgit status",
        cmd_status,
        true
    });

    d.register_command({
        "add",
        "Add file contents to the staging index",
        "lgit add <path>... | lgit add . | lgit add -A",
        cmd_add,
        true
    });

    d.register_command({
        "commit",
        "Record changes to the repository",
        "lgit commit -m \"<message>\" [--amend]",
        cmd_commit,
        true
    });

    d.register_command({
        "log",
        "Show commit history",
        "lgit log [--oneline] [-n <count>]",
        cmd_log,
        true
    });

    d.register_command({
        "diff",
        "Show changes between commits, commit and working tree, etc.",
        "lgit diff [--staged | --cached]",
        cmd_diff,
        true
    });

    d.register_command({
        "branch",
        "List, create, or delete branches",
        "lgit branch [<branch_name>] [-d <branch_name>]",
        cmd_branch,
        true
    });

    d.register_command({
        "switch",
        "Switch branches safely",
        "lgit switch <branch> | lgit switch -c <new_branch>",
        cmd_switch,
        true
    });

    d.register_command({
        "restore",
        "Restore working tree files or unstage files",
        "lgit restore <path>... [--staged]",
        cmd_restore,
        true
    });

    d.register_command({
        "reset",
        "Reset current HEAD to specified state",
        "lgit reset [--soft | --mixed | --hard] [<commit>]",
        cmd_reset,
        true
    });

    d.register_command({
        "show",
        "Show various types of objects (commits, diffs)",
        "lgit show [<commit>]",
        cmd_show,
        true
    });

    d.register_command({
        "rm",
        "Remove files from the working tree and from the index",
        "lgit rm <path>... [--cached]",
        cmd_rm,
        true
    });

    d.register_command({
        "mv",
        "Move or rename a file, directory, or symlink",
        "lgit mv <source> <destination>",
        cmd_mv,
        true
    });

    d.register_command({
        "check-ignore",
        "Debug .light-gitignore rules for paths",
        "lgit check-ignore <path>...",
        cmd_check_ignore,
        true
    });

    d.register_command({
        "config",
        "Get and set repository or global options",
        "lgit config <key> [<value>] [--global] [--list]",
        cmd_config,
        true
    });

    d.register_command({
        "gc",
        "Cleanup unnecessary files and optimize local repository",
        "lgit gc",
        cmd_gc,
        true
    });

    d.register_command({
        "fsck",
        "Verify the connectivity and validity of objects in database",
        "lgit fsck",
        cmd_fsck,
        true
    });

    d.register_command({
        "version",
        "Display light-git version and build information",
        "lgit version",
        cmd_version,
        true
    });

    d.register_command({
        "tag",
        "Create, list, or delete tag references",
        "lgit tag [<name>] [<commit>] [-d <name>] [-a <name> -m <msg>]",
        cmd_tag,
        true
    });

    d.register_command({
        "stash",
        "Stash changes in a dirty working directory and restore them",
        "lgit stash [push [-m <msg>]] | lgit stash list | lgit stash pop | lgit stash apply | lgit stash drop | lgit stash clear",
        cmd_stash,
        true
    });

    d.register_command({
        "merge",
        "Join two or more development histories together",
        "lgit merge <branch>",
        cmd_merge,
        true
    });

    d.register_command({
        "cherry-pick",
        "Apply the changes introduced by an existing commit",
        "lgit cherry-pick <commit>",
        cmd_cherry_pick,
        true
    });

    d.register_command({
        "export-git",
        "Export light-git repository to standard Git fast-import format",
        "lgit export-git [--target <dir>] [--output <file.fi>]",
        cmd_export_git,
        true
    });

    d.register_command({
        "remote",
        "Manage set of tracked repositories",
        "lgit remote [add <name> <url> | remove <name> | get-url <name> | set-url <name> <url> | -v]",
        cmd_remote,
        true
    });

    d.register_command({
        "fetch",
        "Download objects and refs from another repository",
        "lgit fetch [<remote>] [<branch>]",
        cmd_fetch,
        true
    });

    d.register_command({
        "push",
        "Update remote refs along with associated objects",
        "lgit push [<remote>] [<branch>] [--force]",
        cmd_push,
        true
    });

    d.register_command({
        "pull",
        "Fetch from and integrate with another repository",
        "lgit pull [<remote>] [<branch>]",
        cmd_pull,
        true
    });

    d.register_command({
        "clone",
        "Clone a repository or bundle into a new directory",
        "lgit clone <source> [<directory>] [-b <branch>] [--depth <n>]",
        cmd_clone,
        true
    });

    d.register_command({
        "bundle",
        "Package repository objects into portable .lgitpack archives",
        "lgit bundle <create|verify|unbundle|list-heads> <file.lgitpack> [<rev>...]",
        cmd_bundle,
        true
    });

    d.register_command({
        "worktree",
        "Manage multiple working trees attached to same repository",
        "lgit worktree <list | add <path> [<branch>] | remove <path> [--force] | prune>",
        cmd_worktree,
        true
    });

    // Next-generation roadmap commands
    const std::vector<std::pair<std::string, std::string>> future_cmds = {
        {"rebase", "Reapply commits on top of another base tip"},
        {"ui", "Launch embedded web dashboard and visual commit graph viewer"},
        {"sign", "Sign commits and tags with cryptographic Ed25519 keys"},
        {"daemon", "Peer-to-peer and LAN repository discovery daemon"}
    };

    for (const auto& [name, desc] : future_cmds) {
        d.register_command({
            name,
            desc + " (planned for future release)",
            "lgit " + name,
            cmd_future_stub,
            false
        });
    }
}

} // namespace commands
} // namespace light_git
