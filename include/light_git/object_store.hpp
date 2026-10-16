#pragma once

#include "light_git/types.hpp"
#include <filesystem>
#include <vector>
#include <cstdint>
#include <string_view>
#include <optional>

namespace light_git {

struct RawObject {
    ObjectType type{ObjectType::Invalid};
    std::vector<uint8_t> data;
};

class ObjectStore {
public:
    explicit ObjectStore(std::filesystem::path objects_dir);

    // Writes an object to the store. Returns its content-addressed ObjectId.
    ObjectId write_object(ObjectType type, const void* data, size_t size);
    ObjectId write_object(ObjectType type, const std::vector<uint8_t>& data);
    ObjectId write_object(ObjectType type, std::string_view str);

    // Reads and validates an object from the store. Throws on corruption or missing object.
    RawObject read_object(const ObjectId& id) const;

    // Checks if an object exists in the store
    bool has_object(const ObjectId& id) const;

    // Lists all object IDs currently present in the store
    std::vector<ObjectId> list_all_objects() const;

    // Removes an object (used by garbage collection)
    bool remove_object(const ObjectId& id);

    const std::filesystem::path& objects_dir() const { return objects_dir_; }

    // Helpers to resolve short hash prefixes (e.g. "a31f9c2")
    std::optional<ObjectId> resolve_prefix(std::string_view prefix) const;

private:
    std::filesystem::path object_path(const ObjectId& id) const;
    std::filesystem::path objects_dir_;
};

} // namespace light_git
