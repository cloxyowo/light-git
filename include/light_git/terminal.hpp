#pragma once

#include <string>
#include <string_view>
#include <iostream>

namespace light_git {
namespace term {

class Terminal {
public:
    static void init();
    static void set_color_enabled(bool enabled);
    static bool is_color_enabled();
    static void set_unicode_enabled(bool enabled);
    static bool is_unicode_enabled();
    static bool is_stdout_tty();

    // Palette: Purple / Violet themed
    static std::string purple(std::string_view text);
    static std::string violet(std::string_view text);
    static std::string bold_purple(std::string_view text);
    static std::string bold_violet(std::string_view text);
    static std::string bold(std::string_view text);
    static std::string dim(std::string_view text);
    static std::string red(std::string_view text);
    static std::string green(std::string_view text);
    static std::string yellow(std::string_view text);
    static std::string cyan(std::string_view text);

    // Box drawing & symbols
    static std::string sym_branch();
    static std::string sym_bullet();
    static std::string sym_check();
    static std::string sym_cross();
    static std::string sym_arrow();
    static std::string sym_pipe();
    static std::string sym_corner();
    static std::string sym_line(size_t len = 30);

    // UI blocks
    static void print_banner(std::string_view version);
    static void print_tip(std::string_view tip_text);
    static void print_error(std::string_view error_text);
    static void print_success(std::string_view success_text);
    static void print_warning(std::string_view warning_text);
    static void print_key_value(std::string_view key, std::string_view val, size_t key_width = 10);

private:
    static bool color_enabled_;
    static bool unicode_enabled_;
    static bool is_tty_;
};

} // namespace term
} // namespace light_git
