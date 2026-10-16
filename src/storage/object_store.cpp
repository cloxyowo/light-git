#include "light_git/object_store.hpp"
#include "light_git/crypto.hpp"
#include "light_git/filesystem.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace light_git {

ObjectStore::ObjectStore(std::filesystem::path objects_dir)
    : objects_dir_(std::move(objects_dir)) {
}

std::filesystem::path ObjectStore::object_path(const ObjectId& id) const {
    std::string hex = id.to_hex();
    std::string dir_name = hex.substr(0, 2);
    std::string file_name = hex.substr(2);
    return objects_dir_ / dir_name / file_name;
}

ObjectId ObjectStore::write_object(ObjectType type, const void* data, size_t size) {
    std::string_view type_sv = to_string(type);
    std::string header = std::string(type_sv) + " " + std::to_string(size);
    header.push_back('\0');

    // Create complete raw object buffer: header + data
    std::vector<uint8_t> buffer;
    buffer.reserve(header.size() + size);
    buffer.insert(buffer.end(), header.begin(), header.end());
    const uint8_t* p = static_cast<const uint8_t*>(data);
    if (size > 0 && p != nullptr) {
        buffer.insert(buffer.end(), p, p + size);
    }

    ObjectId id = hash_bytes(buffer.data(), buffer.size());
    std::filesystem::path path = object_path(id);

    // If already exists, content is identical (deduplicated)
    if (std::filesystem::exists(path)) {
        return id;
    }

    fs_util::atomic_write_file(path, buffer.data(), buffer.size());
    return id;
}

ObjectId ObjectStore::write_object(ObjectType type, const std::vector<uint8_t>& data) {
    return write_object(type, data.data(), data.size());
}

ObjectId ObjectStore::write_object(ObjectType type, std::string_view str) {
    return write_object(type, str.data(), str.size());
}

RawObject ObjectStore::read_object(const ObjectId& id) const {
    std::filesystem::path path = object_path(id);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Object not found: " + id.to_hex());
    }

    std::vector<uint8_t> buffer = fs_util::read_file_bytes(path);

    // Verify SHA-256 integrity
    ObjectId computed_id = hash_bytes(buffer.data(), buffer.size());
    if (computed_id != id) {
        throw std::runtime_error("Object integrity error: SHA-256 mismatch for " + id.to_hex() + 
                                 " (computed " + computed_id.to_hex() + ")");
    }

    // Parse header: "<type> <size>\0<payload>"
    auto null_it = std::find(buffer.begin(), buffer.end(), 0);
    if (null_it == buffer.end()) {
        throw std::runtime_error("Malformed object format: missing null terminator in header for " + id.to_hex());
    }

    size_t header_len = std::distance(buffer.begin(), null_it);
    std::string header_str(buffer.begin(), null_it);

    size_t space_pos = header_str.find(' ');
    if (space_pos == std::string::npos) {
        throw std::runtime_error("Malformed object header: " + header_str);
    }

    std::string type_str = header_str.substr(0, space_pos);
    std::string size_str = header_str.substr(space_pos + 1);

    ObjectType type = object_type_from_string(type_str);
    if (type == ObjectType::Invalid) {
        throw std::runtime_error("Invalid object type: " + type_str);
    }

    size_t declared_size = 0;
    try {
        declared_size = std::stoull(size_str);
    } catch (...) {
        throw std::runtime_error("Invalid object size in header: " + size_str);
    }

    size_t payload_offset = header_len + 1;
    size_t actual_payload_size = buffer.size() - payload_offset;

    if (declared_size != actual_payload_size) {
        throw std::runtime_error("Object payload size mismatch for " + id.to_hex() +
                                 " (expected " + std::to_string(declared_size) +
                                 ", got " + std::to_string(actual_payload_size) + ")");
    }

    RawObject obj;
    obj.type = type;
    obj.data.assign(buffer.begin() + payload_offset, buffer.end());
    return obj;
}

bool ObjectStore::has_object(const ObjectId& id) const {
    return std::filesystem::exists(object_path(id));
}

std::vector<ObjectId> ObjectStore::list_all_objects() const {
    std::vector<ObjectId> result;
    if (!std::filesystem::exists(objects_dir_)) {
        return result;
    }

    std::error_code ec;
    for (const auto& dir_entry : std::filesystem::directory_iterator(objects_dir_, ec)) {
        if (!dir_entry.is_directory(ec)) continue;
        std::string prefix = dir_entry.path().filename().string();
        if (prefix.size() != 2) continue;

        for (const auto& file_entry : std::filesystem::directory_iterator(dir_entry.path(), ec)) {
            if (!file_entry.is_regular_file(ec)) continue;
            std::string rest = file_entry.path().filename().string();
            if (rest.size() != 62) continue;

            std::string hex = prefix + rest;
            try {
                result.push_back(ObjectId::from_hex(hex));
            } catch (...) {
                // Ignore invalid file names
            }
        }
    }

    return result;
}

bool ObjectStore::remove_object(const ObjectId& id) {
    std::filesystem::path path = object_path(id);
    std::error_code ec;
    bool removed = std::filesystem::remove(path, ec);
    // If parent directory is now empty, remove it too
    if (removed && path.has_parent_path()) {
        std::filesystem::remove(path.parent_path(), ec);
    }
    return removed;
}

std::optional<ObjectId> ObjectStore::resolve_prefix(std::string_view prefix) const {
    if (prefix.size() == 64) {
        try {
            ObjectId id = ObjectId::from_hex(prefix);
            if (has_object(id)) return id;
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    if (prefix.size() < 4) {
        throw std::runtime_error("Prefix too short to resolve (minimum 4 characters): " + std::string(prefix));
    }

    std::string dir_part = std::string(prefix.substr(0, 2));
    std::filesystem::path sub_dir = objects_dir_ / dir_part;
    if (!std::filesystem::exists(sub_dir)) {
        return std::nullopt;
    }

    std::string rest_prefix = std::string(prefix.substr(2));
    std::vector<ObjectId> matches;

    std::error_code ec;
    for (const auto& file_entry : std::filesystem::directory_iterator(sub_dir, ec)) {
        std::string fname = file_entry.path().filename().string();
        if (fname.rfind(rest_prefix, 0) == 0 && fname.size() == 62) {
            matches.push_back(ObjectId::from_hex(dir_part + fname));
        }
    }

    if (matches.size() == 1) {
        return matches.front();
    }
    if (matches.size() > 1) {
        throw std::runtime_error("Ambiguous object prefix: " + std::string(prefix));
    }
    return std::nullopt;
}

} // namespace light_git
