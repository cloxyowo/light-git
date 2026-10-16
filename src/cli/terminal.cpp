#include "light_git/terminal.hpp"
#include <cstdlib>
#include <iostream>
#include <iomanip>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <cstdio>
#endif

namespace light_git {
namespace term {

bool Terminal::color_enabled_ = true;
bool Terminal::unicode_enabled_ = true;
bool Terminal::is_tty_ = false;

void Terminal::init() {
#if defined(_WIN32)
    is_tty_ = (_isatty(_fileno(stdout)) != 0);
    if (is_tty_) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        // Check output code page for unicode support
        UINT cp = GetConsoleOutputCP();
        unicode_enabled_ = (cp == 65001); // UTF-8
    } else {
        unicode_enabled_ = false;
    }
#else
    is_tty_ = (isatty(fileno(stdout)) != 0);
    const char* lang = std::getenv("LANG");
    const char* lc_all = std::getenv("LC_ALL");
    std::string env_str = (lc_all ? lc_all : (lang ? lang : ""));
    unicode_enabled_ = (env_str.find("UTF-8") != std::string::npos || env_str.find("utf8") != std::string::npos);
#endif

    // Check NO_COLOR specification (https://no-color.org)
    const char* no_color = std::getenv("NO_COLOR");
    if (no_color != nullptr && no_color[0] != '\0') {
        color_enabled_ = false;
    } else if (!is_tty_) {
        // If redirected and NO_COLOR not specified, default to no color to keep pipes clean
        color_enabled_ = false;
    }
}

void Terminal::set_color_enabled(bool enabled) {
    color_enabled_ = enabled;
}

bool Terminal::is_color_enabled() {
    return color_enabled_;
}

void Terminal::set_unicode_enabled(bool enabled) {
    unicode_enabled_ = enabled;
}

bool Terminal::is_unicode_enabled() {
    return unicode_enabled_;
}

bool Terminal::is_stdout_tty() {
    return is_tty_;
}

static std::string colorize(std::string_view text, const char* code, bool enabled) {
    if (!enabled || text.empty()) {
        return std::string(text);
    }
    std::string out;
    out.reserve(text.size() + 16);
    out.append(code);
    out.append(text);
    out.append("\033[0m");
    return out;
}

// Violet/Purple ANSI 256 palette: 141 (soft violet), 135 (rich purple)
std::string Terminal::violet(std::string_view text) {
    return colorize(text, "\033[38;5;141m", color_enabled_);
}

std::string Terminal::purple(std::string_view text) {
    return colorize(text, "\033[38;5;135m", color_enabled_);
}

std::string Terminal::bold_violet(std::string_view text) {
    return colorize(text, "\033[1;38;5;141m", color_enabled_);
}

std::string Terminal::bold_purple(std::string_view text) {
    return colorize(text, "\033[1;38;5;135m", color_enabled_);
}

std::string Terminal::bold(std::string_view text) {
    return colorize(text, "\033[1m", color_enabled_);
}

std::string Terminal::dim(std::string_view text) {
    return colorize(text, "\033[2m", color_enabled_);
}

std::string Terminal::red(std::string_view text) {
    return colorize(text, "\033[31m", color_enabled_);
}

std::string Terminal::green(std::string_view text) {
    return colorize(text, "\033[32m", color_enabled_);
}

std::string Terminal::yellow(std::string_view text) {
    return colorize(text, "\033[33m", color_enabled_);
}

std::string Terminal::cyan(std::string_view text) {
    return colorize(text, "\033[36m", color_enabled_);
}

std::string Terminal::sym_branch() {
    return unicode_enabled_ ? "\xE2\x94\x80" : "-"; // ─ or -
}

std::string Terminal::sym_bullet() {
    return unicode_enabled_ ? "\xE2\x80\xA2" : "*"; // • or *
}

std::string Terminal::sym_check() {
    return unicode_enabled_ ? "\xE2\x9C\x93" : "+"; // ✓ or +
}

std::string Terminal::sym_cross() {
    return unicode_enabled_ ? "\xE2\x9C\x97" : "x"; // ✗ or x
}

std::string Terminal::sym_arrow() {
    return unicode_enabled_ ? "\xE2\x86\x92" : "->"; // → or ->
}

std::string Terminal::sym_pipe() {
    return unicode_enabled_ ? "\xE2\x94\x82" : "|"; // │ or |
}

std::string Terminal::sym_corner() {
    return unicode_enabled_ ? "\xE2\x94\x94\xE2\x94\x80" : "`-"; // └─ or `-
}

std::string Terminal::sym_line(size_t len) {
    if (!unicode_enabled_) {
        return std::string(len, '-');
    }
    std::string s;
    s.reserve(len * 3);
    for (size_t i = 0; i < len; ++i) {
        s.append("\xE2\x94\x80");
    }
    return s;
}

void Terminal::print_banner(std::string_view version) {
    std::cout << bold_purple("light-git") << "  " << violet(std::string("v") + std::string(version)) << "\n";
    std::cout << dim("lightweight version control") << "\n\n";
}

void Terminal::print_tip(std::string_view tip_text) {
    std::cout << dim("tip: ") << violet(tip_text) << "\n";
}

void Terminal::print_error(std::string_view error_text) {
    std::cerr << red("error: ") << error_text << "\n";
}

void Terminal::print_success(std::string_view success_text) {
    std::cout << green(success_text) << "\n";
}

void Terminal::print_warning(std::string_view warning_text) {
    std::cout << yellow("warning: ") << warning_text << "\n";
}

void Terminal::print_key_value(std::string_view key, std::string_view val, size_t key_width) {
    std::cout << dim(key);
    if (key.size() < key_width) {
        std::cout << std::string(key_width - key.size(), ' ');
    } else {
        std::cout << " ";
    }
    std::cout << val << "\n";
}

} // namespace term
} // namespace light_git
