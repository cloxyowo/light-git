#include "light_git/filesystem.hpp"
#include <fstream>
#include <stdexcept>
#include <random>
#include <chrono>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace light_git {
namespace fs_util {

std::string normalize_path(const fs::path& p) {
    std::string s = p.lexically_normal().generic_string();
    // Strip trailing slashes unless root
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
    // Remove leading "./" if present
    if (s.rfind("./", 0) == 0) {
        s = s.substr(2);
    }
    return s;
}

std::string to_repo_relative(const fs::path& base_dir, const fs::path& target_path) {
    fs::path abs_base = fs::absolute(base_dir).lexically_normal();
    fs::path abs_target = fs::absolute(target_path).lexically_normal();

    auto rel = abs_target.lexically_relative(abs_base);
    std::string rel_str = rel.generic_string();

    if (rel_str.empty()) {
        return ".";
    }
    if (rel_str.rfind("..", 0) == 0) {
        throw std::runtime_error("Path '" + target_path.string() + "' is outside repository working tree");
    }
    return normalize_path(rel);
}

bool is_path_inside(const fs::path& base_dir, const fs::path& target_path) {
    try {
        fs::path abs_base = fs::absolute(base_dir).lexically_normal();
        fs::path abs_target = fs::absolute(target_path).lexically_normal();
        auto rel = abs_target.lexically_relative(abs_base);
        std::string rel_str = rel.generic_string();
        return (rel_str != ".." && rel_str.rfind("../", 0) != 0);
    } catch (...) {
        return false;
    }
}

std::vector<uint8_t> read_file_bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for reading: " + path.string());
    }
    auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Could not determine size of file: " + path.string());
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (size > 0) {
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Failed to read file contents: " + path.string());
        }
    }
    return buffer;
}

std::string read_file_string(const fs::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for reading: " + path.string());
    }
    auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Could not determine size of file: " + path.string());
    }
    std::string buffer(static_cast<size_t>(size), '\0');
    file.seekg(0, std::ios::beg);
    if (size > 0) {
        if (!file.read(&buffer[0], size)) {
            throw std::runtime_error("Failed to read file contents: " + path.string());
        }
    }
    return buffer;
}

void write_file_bytes(const fs::path& path, const void* data, size_t size) {
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + path.string());
    }
    if (size > 0 && data != nullptr) {
        file.write(reinterpret_cast<const char*>(data), size);
        if (!file.good()) {
            throw std::runtime_error("Failed writing data to file: " + path.string());
        }
    }
}

void write_file_string(const fs::path& path, std::string_view content) {
    write_file_bytes(path, content.data(), content.size());
}

void atomic_write_file(const fs::path& dest_path, const void* data, size_t size) {
    if (dest_path.has_parent_path()) {
        fs::create_directories(dest_path.parent_path());
    }

    // Generate unique temp filename in same directory to ensure same filesystem for atomic rename
    static thread_local std::mt19937_64 rng(std::random_device{}());
    uint64_t rand_val = rng();
    fs::path temp_path = dest_path;
    temp_path += ".tmp." + std::to_string(rand_val);

    try {
        write_file_bytes(temp_path, data, size);

#if defined(_WIN32)
        std::wstring wtemp = temp_path.wstring();
        std::wstring wdest = dest_path.wstring();
        if (!MoveFileExW(wtemp.c_str(), wdest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DWORD err = GetLastError();
            std::error_code ec;
            fs::remove(temp_path, ec);
            throw std::runtime_error("MoveFileExW failed with error " + std::to_string(err) + " for " + dest_path.string());
        }
#else
        std::error_code ec;
        fs::rename(temp_path, dest_path, ec);
        if (ec) {
            fs::remove(temp_path, ec);
            throw std::runtime_error("Rename failed: " + ec.message() + " for " + dest_path.string());
        }
#endif
    } catch (...) {
        std::error_code ec;
        fs::remove(temp_path, ec);
        throw;
    }
}

void atomic_write_file(const fs::path& dest_path, std::string_view content) {
    atomic_write_file(dest_path, content.data(), content.size());
}

bool is_binary(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t check_len = (size < 8192) ? size : 8192;
    for (size_t i = 0; i < check_len; ++i) {
        if (bytes[i] == 0) {
            return true;
        }
    }
    return false;
}

bool is_binary_file(const fs::path& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        char buffer[8192];
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = file.gcount();
        return is_binary(buffer, static_cast<size_t>(bytes_read));
    } catch (...) {
        return false;
    }
}

uint32_t get_file_mode(const fs::path& path) {
    try {
        auto status = fs::status(path);
        if (fs::is_symlink(status)) {
            return 0120000;
        }
        if (fs::is_directory(status)) {
            return 0040000;
        }
#if defined(_WIN32)
        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));
        if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".ps1") {
            return 0100755;
        }
        return 0100644;
#else
        auto perms = status.permissions();
        if ((perms & fs::perms::owner_exec) != fs::perms::none) {
            return 0100755;
        }
        return 0100644;
#endif
    } catch (...) {
        return 0100644;
    }
}

void set_file_mode(const fs::path& path, uint32_t mode) {
#if !defined(_WIN32)
    fs::perms perms = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read;
    if ((mode & 0111) != 0) {
        perms |= fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
    }
    std::error_code ec;
    fs::permissions(path, perms, ec);
#else
    (void)path;
    (void)mode;
#endif
}

uint64_t get_mtime_ms(const fs::path& path) {
    try {
        auto ftime = fs::last_write_time(path);
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(ftime.time_since_epoch());
        return static_cast<uint64_t>(dur.count());
    } catch (...) {
        return 0;
    }
}

std::vector<DirEntry> list_directory_recursive(const fs::path& root_dir) {
    std::vector<DirEntry> entries;
    if (!fs::exists(root_dir) || !fs::is_directory(root_dir)) {
        return entries;
    }

    std::error_code ec;
    fs::recursive_directory_iterator iter(root_dir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    while (iter != end) {
        const auto& item = *iter;
        fs::path rel = item.path().lexically_relative(root_dir);
        std::string rel_str = rel.generic_string();

        // Skip internal .light-git or .git metadata directories
        if (rel_str == ".light-git" || rel_str.rfind(".light-git/", 0) == 0 ||
            rel_str == ".git" || rel_str.rfind(".git/", 0) == 0) {
            iter.disable_recursion_pending();
            iter.increment(ec);
            continue;
        }

        bool is_dir = item.is_directory(ec);
        bool is_sym = item.is_symlink(ec);
        uint64_t sz = 0;
        uint64_t mt = 0;

        if (!is_dir && !is_sym) {
            sz = item.file_size(ec);
            mt = get_mtime_ms(item.path());
        }

        entries.push_back({rel, item.path(), is_dir, is_sym, sz, mt});
        iter.increment(ec);
    }

    return entries;
}

} // namespace fs_util
} // namespace light_git
