#pragma once

#include "light_git/types.hpp"
#include "light_git/object_store.hpp"
#include <string>
#include <vector>
#include <map>
#include <string_view>

namespace light_git {

struct TreeEntry {
    uint32_t mode{0100644};
    ObjectType type{ObjectType::Blob};
    std::string name;
    ObjectId id;

    bool operator<(const TreeEntry& other) const {
        return name < other.name;
    }
};

class Tree {
public:
    Tree() = default;

    void add_entry(TreeEntry entry);
    const std::vector<TreeEntry>& entries() const { return entries_; }
    const TreeEntry* find_entry(std::string_view name) const;

    std::vector<uint8_t> serialize() const;
    static Tree deserialize(const std::vector<uint8_t>& data);

    // Recursively flatten tree into map of "dir/subdir/file" -> TreeEntry
    static std::map<std::string, TreeEntry> flatten_tree(const ObjectStore& store, const ObjectId& root_tree_id);

private:
    std::vector<TreeEntry> entries_;
};

} // namespace light_git
