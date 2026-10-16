#include "light_git/cli.hpp"
#include "light_git/terminal.hpp"
#include "commands/commands.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace light_git {

bool CommandContext::has_flag(const std::string& flag) const {
    return std::find(flags.begin(), flags.end(), flag) != flags.end();
}

std::string CommandContext::get_option(const std::string& opt, const std::string& default_val) const {
    auto it = options.find(opt);
    if (it != options.end()) {
        return it->second;
    }
    return default_val;
}

CliDispatcher::CliDispatcher() {
    commands::register_all_commands(*this);
}

void CliDispatcher::register_command(CommandDef cmd) {
    commands_[cmd.name] = std::move(cmd);
}

void CliDispatcher::print_version() const {
    term::Terminal::print_banner("0.2.0");
    std::cout << "built with C++17 for high-performance minimal footprint\n";
#if defined(_WIN32)
    std::cout << "platform: Windows (MSVC)\n";
#elif defined(__linux__)
    std::cout << "platform: Linux (GCC/Clang)\n";
#elif defined(__APPLE__)
    std::cout << "platform: macOS (Clang)\n";
#else
    std::cout << "platform: Unknown\n";
#endif
}

void CliDispatcher::print_general_help() const {
    using namespace term;
    Terminal::print_banner("0.2.0");

    std::cout << Terminal::bold("USAGE:") << "\n";
    std::cout << "  lgit [--version] [--help] [--no-color] <command> [<args>]\n\n";

    std::cout << Terminal::bold("CORE COMMANDS:") << "\n";
    std::vector<std::string> core_cmds = {
        "init", "status", "add", "commit", "log", "diff",
        "branch", "switch", "restore", "reset", "show", "rm", "mv",
        "tag", "stash", "merge", "cherry-pick", "export-git",
        "check-ignore", "config", "gc", "fsck"
    };

    for (const auto& cname : core_cmds) {
        auto it = commands_.find(cname);
        if (it != commands_.end()) {
            std::cout << "  " << Terminal::bold_violet(it->first);
            if (it->first.size() < 16) {
                std::cout << std::string(16 - it->first.size(), ' ');
            } else {
                std::cout << " ";
            }
            std::cout << it->second.description << "\n";
        }
    }

    std::cout << "\n" << Terminal::bold("PLANNED / ROADMAP COMMANDS:") << "\n";
    std::vector<std::string> future_cmds = {
        "clone", "fetch", "pull", "push", "remote",
        "rebase", "worktree", "ui", "sign", "bundle", "daemon"
    };

    for (const auto& cname : future_cmds) {
        auto it = commands_.find(cname);
        if (it != commands_.end()) {
            std::cout << "  " << Terminal::dim(it->first);
            if (it->first.size() < 16) {
                std::cout << std::string(16 - it->first.size(), ' ');
            } else {
                std::cout << " ";
            }
            std::cout << Terminal::dim(it->second.description) << "\n";
        }
    }

    std::cout << "\n" << Terminal::dim("Run 'lgit help <command>' for detailed help on any command.") << "\n";
}

void CliDispatcher::print_command_help(const std::string& cmd_name) const {
    using namespace term;
    auto it = commands_.find(cmd_name);
    if (it == commands_.end()) {
        Terminal::print_error("Unknown command '" + cmd_name + "'");
        return;
    }

    std::cout << Terminal::bold_violet("lgit " + it->first) << " - " << it->second.description << "\n\n";
    std::cout << Terminal::bold("USAGE:") << "\n  " << it->second.usage << "\n\n";
    if (!it->second.is_implemented) {
        std::cout << Terminal::yellow("Note:") << " This command is part of the future light-git roadmap.\n\n";
    }
}

int CliDispatcher::dispatch(int argc, const char* const argv[]) {
    term::Terminal::init();

    if (argc <= 1) {
        print_general_help();
        return 0;
    }

    CommandContext ctx;
    std::string cmd_name;
    int arg_idx = 1;

    // Parse global flags before command
    while (arg_idx < argc) {
        std::string_view arg = argv[arg_idx];
        if (arg == "--no-color") {
            term::Terminal::set_color_enabled(false);
            ctx.no_color = true;
            arg_idx++;
        } else if (arg == "--version" || arg == "-v") {
            print_version();
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            if (arg_idx + 1 < argc) {
                print_command_help(argv[arg_idx + 1]);
            } else {
                print_general_help();
            }
            return 0;
        } else if (arg == "version") {
            print_version();
            return 0;
        } else if (arg == "help") {
            if (arg_idx + 1 < argc) {
                print_command_help(argv[arg_idx + 1]);
            } else {
                print_general_help();
            }
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            cmd_name = arg;
            arg_idx++;
            break;
        } else {
            term::Terminal::print_error("Unknown option: " + std::string(arg));
            return 1;
        }
    }

    if (cmd_name.empty()) {
        print_general_help();
        return 0;
    }

    ctx.command_name = cmd_name;

    // Parse command arguments, options, and flags
    while (arg_idx < argc) {
        std::string arg = argv[arg_idx];

        if (arg == "--help" || arg == "-h") {
            print_command_help(cmd_name);
            return 0;
        }

        if (arg == "--no-color") {
            term::Terminal::set_color_enabled(false);
            ctx.no_color = true;
            arg_idx++;
            continue;
        }

        if (arg.rfind("--", 0) == 0) {
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                std::string key = arg.substr(2, eq - 2);
                std::string val = arg.substr(eq + 1);
                ctx.options[key] = val;
            } else {
                std::string key = arg.substr(2);
                // Check if next argument is a value (does not start with '-')
                if (arg_idx + 1 < argc && argv[arg_idx + 1][0] != '-') {
                    ctx.options[key] = argv[arg_idx + 1];
                    arg_idx++;
                } else {
                    ctx.flags.push_back(key);
                }
            }
        } else if (arg.size() > 1 && arg[0] == '-') {
            // Short options
            if (arg == "-m" || arg == "-c" || arg == "-d" || arg == "-D") {
                std::string key = arg.substr(1);
                if (arg_idx + 1 < argc && argv[arg_idx + 1][0] != '-') {
                    ctx.options[key] = argv[arg_idx + 1];
                    arg_idx++;
                } else {
                    ctx.flags.push_back(key);
                }
            } else {
                // Short flags like -A, -s
                for (size_t i = 1; i < arg.size(); ++i) {
                    ctx.flags.push_back(std::string(1, arg[i]));
                }
            }
        } else {
            ctx.args.push_back(arg);
        }
        arg_idx++;
    }

    auto it = commands_.find(cmd_name);
    if (it == commands_.end()) {
        term::Terminal::print_error("'" + cmd_name + "' is not a light-git command. See 'lgit --help'.");
        return 1;
    }

    try {
        return it->second.handler(ctx);
    } catch (const std::exception& ex) {
        term::Terminal::print_error(ex.what());
        return 1;
    } catch (...) {
        term::Terminal::print_error("An unexpected error occurred.");
        return 1;
    }
}

} // namespace light_git
