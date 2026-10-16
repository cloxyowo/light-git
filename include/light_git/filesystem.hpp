#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <optional>

namespace light_git {
namespace fs_util {

namespace fs = std::filesystem;

// Path normalization: uses '/' as separator, eliminates redundant '.', and checks for directory traversal
std::string normalize_path(const fs::path& p);
std::string to_repo_relative(const fs::path& base_dir, const fs::path& target_path);
bool is_path_inside(const fs::path& base_dir, const fs::path& target_path);

// File I/O
std::vector<uint8_t> read_file_bytes(const fs::path& path);
std::string read_file_string(const fs::path& path);
void write_file_bytes(const fs::path& path, const void* data, size_t size);
void write_file_string(const fs::path& path, std::string_view content);

// Atomic write: writes to temp file first, then atomically replaces destination
void atomic_write_file(const fs::path& dest_path, const void* data, size_t size);
void atomic_write_file(const fs::path& dest_path, std::string_view content);

// Metadata & checks
bool is_binary(const void* data, size_t size);
bool is_binary_file(const fs::path& path);
uint32_t get_file_mode(const fs::path& path);
void set_file_mode(const fs::path& path, uint32_t mode);
uint64_t get_mtime_ms(const fs::path& path);

// Directory traversal
struct DirEntry {
    fs::path relative_path;
    fs::path full_path;
    bool is_directory;
    bool is_symlink;
    uint64_t size;
    uint64_t mtime_ms;
};

std::vector<DirEntry> list_directory_recursive(const fs::path& root_dir);

} // namespace fs_util
} // namespace light_git
