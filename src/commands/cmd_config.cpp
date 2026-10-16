#include "commands.hpp"
#include "light_git/repository.hpp"
#include "light_git/terminal.hpp"
#include <iostream>

namespace light_git {
namespace commands {

int cmd_config(const CommandContext& ctx) {
    using namespace term;

    bool is_global = ctx.has_flag("global");
    bool is_list = ctx.has_flag("list") || ctx.has_flag("l");

    if (is_list) {
        if (is_global) {
            Config global_cfg;
            global_cfg.load_file(Config::get_global_config_path());
            for (const auto& [k, v] : global_cfg.get_all()) {
                std::cout << k << "=" << v << "\n";
            }
        } else {
            try {
                Repository repo = Repository::open(".");
                for (const auto& [k, v] : repo.config().get_all_merged()) {
                    std::cout << k << "=" << v << "\n";
                }
            } catch (...) {
                Config global_cfg;
                global_cfg.load_file(Config::get_global_config_path());
                for (const auto& [k, v] : global_cfg.get_all()) {
                    std::cout << k << "=" << v << "\n";
                }
            }
        }
        return 0;
    }

    if (ctx.args.empty()) {
        Terminal::print_error("No configuration key specified.");
        return 1;
    }

    std::string key = ctx.args[0];

    if (ctx.args.size() == 1) {
        // Read key
        if (is_global) {
            Config global_cfg;
            global_cfg.load_file(Config::get_global_config_path());
            auto val = global_cfg.get(key);
            if (val) {
                std::cout << *val << "\n";
                return 0;
            }
        } else {
            try {
                Repository repo = Repository::open(".");
                auto val = repo.config().get(key);
                if (val) {
                    std::cout << *val << "\n";
                    return 0;
                }
            } catch (...) {
                Config global_cfg;
                global_cfg.load_file(Config::get_global_config_path());
                auto val = global_cfg.get(key);
                if (val) {
                    std::cout << *val << "\n";
                    return 0;
                }
            }
        }
        return 1; // Key not found
    }

    // Set key
    std::string val = ctx.args[1];
    if (is_global) {
        Config global_cfg;
        auto gpath = Config::get_global_config_path();
        global_cfg.load_file(gpath);
        global_cfg.set(key, val);
        global_cfg.save_file(gpath);
    } else {
        Repository repo = Repository::open(".");
        repo.config().set_local(key, val);
    }

    return 0;
}

} // namespace commands
} // namespace light_git
