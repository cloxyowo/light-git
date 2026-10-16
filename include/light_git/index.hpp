#pragma once

#include "light_git/types.hpp"
#include <string>
#include <map>
#include <filesystem>
#include <cstdint>
#include <optional>

namespace light_git {

struct IndexEntry {
    std::string path;
    ObjectId id;
    uint32_t mode{0100644};
    uint64_t size{0};
    uint64_t mtime_ms{0};
    uint16_t stage{0};

    bool operator==(const IndexEntry& other) const {
        return path == other.path && id == other.id && mode == other.mode &&
               size == other.size && mtime_ms == other.mtime_ms && stage == other.stage;
    }
};

class Index {
public:
    Index() = default;

    void add_or_update(IndexEntry entry);
    bool remove(const std::string& path);
    const IndexEntry* find(const std::string& path) const;
    const std::map<std::string, IndexEntry>& entries() const { return entries_; }
    size_t size() const { return entries_.size(); }
    void clear() { entries_.clear(); }

    void load_from_file(const std::filesystem::path& index_path);
    void save_to_file(const std::filesystem::path& index_path) const;

private:
    std::map<std::string, IndexEntry> entries_;
};

} // namespace light_git
