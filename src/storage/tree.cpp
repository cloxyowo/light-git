#include "light_git/tree.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>

namespace light_git {

void Tree::add_entry(TreeEntry entry) {
    auto it = std::find_if(entries_.begin(), entries_.end(), [&](const TreeEntry& e) {
        return e.name == entry.name;
    });
    if (it != entries_.end()) {
        *it = std::move(entry);
    } else {
        entries_.push_back(std::move(entry));
    }
    std::sort(entries_.begin(), entries_.end());
}

const TreeEntry* Tree::find_entry(std::string_view name) const {
    for (const auto& e : entries_) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

std::vector<uint8_t> Tree::serialize() const {
    std::vector<uint8_t> out;
    for (const auto& entry : entries_) {
        std::ostringstream ss;
        ss << std::oct << entry.mode << " " << entry.name;
        std::string prefix = ss.str();
        prefix.push_back('\0');

        out.insert(out.end(), prefix.begin(), prefix.end());
        out.insert(out.end(), entry.id.bytes.begin(), entry.id.bytes.end());
    }
    return out;
}

Tree Tree::deserialize(const std::vector<uint8_t>& data) {
    Tree tree;
    size_t i = 0;
    while (i < data.size()) {
        // Find space
        size_t space_pos = i;
        while (space_pos < data.size() && data[space_pos] != ' ') {
            space_pos++;
        }
        if (space_pos >= data.size()) break;

        std::string mode_str(reinterpret_cast<const char*>(&data[i]), space_pos - i);
        uint32_t mode = 0;
        try {
            mode = static_cast<uint32_t>(std::stoul(mode_str, nullptr, 8));
        } catch (...) {
            throw std::runtime_error("Malformed tree entry mode: " + mode_str);
        }

        // Find null terminator
        size_t null_pos = space_pos + 1;
        while (null_pos < data.size() && data[null_pos] != '\0') {
            null_pos++;
        }
        if (null_pos >= data.size()) break;

        std::string name(reinterpret_cast<const char*>(&data[space_pos + 1]), null_pos - (space_pos + 1));

        // Read 32-byte ObjectId
        size_t id_pos = null_pos + 1;
        if (id_pos + 32 > data.size()) {
            throw std::runtime_error("Truncated tree entry SHA-256 for: " + name);
        }

        ObjectId id;
        std::memcpy(id.bytes.data(), &data[id_pos], 32);

        TreeEntry entry;
        entry.mode = mode;
        entry.name = std::move(name);
        entry.id = id;
        entry.type = ((mode & 0040000) != 0 || mode == 040000) ? ObjectType::Tree : ObjectType::Blob;

        tree.entries_.push_back(std::move(entry));
        i = id_pos + 32;
    }
    std::sort(tree.entries_.begin(), tree.entries_.end());
    return tree;
}

static void flatten_recursive(const ObjectStore& store, const ObjectId& tree_id, const std::string& prefix,
                              std::map<std::string, TreeEntry>& out) {
    RawObject obj = store.read_object(tree_id);
    if (obj.type != ObjectType::Tree) {
        throw std::runtime_error("Expected Tree object, got " + std::string(to_string(obj.type)) + " for " + tree_id.to_hex());
    }

    Tree tree = Tree::deserialize(obj.data);
    for (const auto& entry : tree.entries()) {
        std::string full_path = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.type == ObjectType::Tree) {
            flatten_recursive(store, entry.id, full_path, out);
        } else {
            out[full_path] = entry;
        }
    }
}

std::map<std::string, TreeEntry> Tree::flatten_tree(const ObjectStore& store, const ObjectId& root_tree_id) {
    std::map<std::string, TreeEntry> result;
    if (root_tree_id.is_zero()) {
        return result;
    }
    flatten_recursive(store, root_tree_id, "", result);
    return result;
}

} // namespace light_git
