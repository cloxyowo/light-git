#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>

namespace light_git {

struct CommandContext {
    std::string command_name;
    std::vector<std::string> args;
    std::map<std::string, std::string> options;
    std::vector<std::string> flags;
    bool no_color{false};

    bool has_flag(const std::string& flag) const;
    std::string get_option(const std::string& opt, const std::string& default_val = "") const;
};

using CommandHandler = std::function<int(const CommandContext&)>;

struct CommandDef {
    std::string name;
    std::string description;
    std::string usage;
    CommandHandler handler;
    bool is_implemented{true};
};

class CliDispatcher {
public:
    CliDispatcher();

    void register_command(CommandDef cmd);
    int dispatch(int argc, const char* const argv[]);

    void print_general_help() const;
    void print_command_help(const std::string& cmd_name) const;
    void print_version() const;

private:
    std::map<std::string, CommandDef> commands_;
};

} // namespace light_git
