#include "light_git/index.hpp"
#include "light_git/crypto.hpp"
#include "light_git/filesystem.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace light_git {

namespace {

static constexpr char INDEX_MAGIC[8] = {'L', 'G', 'I', 'T', 'I', 'D', 'X', '\1'};
static constexpr uint32_t INDEX_VERSION = 1;

template <typename T>
void write_val(std::vector<uint8_t>& buf, T val) {
    uint8_t bytes[sizeof(T)];
    std::memcpy(bytes, &val, sizeof(T));
    buf.insert(buf.end(), bytes, bytes + sizeof(T));
}

template <typename T>
T read_val(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + sizeof(T) > end) {
        throw std::runtime_error("Unexpected end of index data");
    }
    T val;
    std::memcpy(&val, ptr, sizeof(T));
    ptr += sizeof(T);
    return val;
}

} // namespace

void Index::add_or_update(IndexEntry entry) {
    entries_[entry.path] = std::move(entry);
}

bool Index::remove(const std::string& path) {
    return entries_.erase(path) > 0;
}

const IndexEntry* Index::find(const std::string& path) const {
    auto it = entries_.find(path);
    if (it != entries_.end()) {
        return &it->second;
    }
    return nullptr;
}

void Index::save_to_file(const std::filesystem::path& index_path) const {
    std::vector<uint8_t> buffer;
    buffer.reserve(64 + entries_.size() * 128);

    // Magic
    buffer.insert(buffer.end(), INDEX_MAGIC, INDEX_MAGIC + sizeof(INDEX_MAGIC));
    // Version
    write_val(buffer, INDEX_VERSION);
    // Entry count
    write_val(buffer, static_cast<uint32_t>(entries_.size()));

    for (const auto& [path, entry] : entries_) {
        uint16_t path_len = static_cast<uint16_t>(entry.path.size());
        write_val(buffer, path_len);
        buffer.insert(buffer.end(), entry.path.begin(), entry.path.end());

        buffer.insert(buffer.end(), entry.id.bytes.begin(), entry.id.bytes.end());
        write_val(buffer, entry.mode);
        write_val(buffer, entry.size);
        write_val(buffer, entry.mtime_ms);
        write_val(buffer, entry.stage);
    }

    // Compute checksum over buffer
    ObjectId checksum = hash_bytes(buffer.data(), buffer.size());
    buffer.insert(buffer.end(), checksum.bytes.begin(), checksum.bytes.end());

    fs_util::atomic_write_file(index_path, buffer.data(), buffer.size());
}

void Index::load_from_file(const std::filesystem::path& index_path) {
    entries_.clear();
    if (!std::filesystem::exists(index_path)) {
        return;
    }

    std::vector<uint8_t> buffer = fs_util::read_file_bytes(index_path);
    if (buffer.size() < sizeof(INDEX_MAGIC) + sizeof(uint32_t) * 2 + 32) {
        throw std::runtime_error("Index file is corrupted or truncated: " + index_path.string());
    }

    // Verify trailing 32-byte SHA-256 checksum
    size_t payload_len = buffer.size() - 32;
    ObjectId expected_checksum;
    std::memcpy(expected_checksum.bytes.data(), buffer.data() + payload_len, 32);

    ObjectId computed_checksum = hash_bytes(buffer.data(), payload_len);
    if (computed_checksum != expected_checksum) {
        throw std::runtime_error("Index file checksum mismatch in " + index_path.string());
    }

    const uint8_t* ptr = buffer.data();
    const uint8_t* end = buffer.data() + payload_len;

    // Check magic
    if (std::memcmp(ptr, INDEX_MAGIC, sizeof(INDEX_MAGIC)) != 0) {
        throw std::runtime_error("Invalid index header magic: " + index_path.string());
    }
    ptr += sizeof(INDEX_MAGIC);

    // Version
    uint32_t version = read_val<uint32_t>(ptr, end);
    if (version != INDEX_VERSION) {
        throw std::runtime_error("Unsupported index format version: " + std::to_string(version));
    }

    // Count
    uint32_t count = read_val<uint32_t>(ptr, end);
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t path_len = read_val<uint16_t>(ptr, end);
        if (ptr + path_len > end) {
            throw std::runtime_error("Index entry path exceeds boundary");
        }
        std::string path(reinterpret_cast<const char*>(ptr), path_len);
        ptr += path_len;

        if (ptr + 32 > end) {
            throw std::runtime_error("Index entry hash truncated");
        }
        ObjectId id;
        std::memcpy(id.bytes.data(), ptr, 32);
        ptr += 32;

        uint32_t mode = read_val<uint32_t>(ptr, end);
        uint64_t size = read_val<uint64_t>(ptr, end);
        uint64_t mtime_ms = read_val<uint64_t>(ptr, end);
        uint16_t stage = read_val<uint16_t>(ptr, end);

        IndexEntry entry;
        entry.path = std::move(path);
        entry.id = id;
        entry.mode = mode;
        entry.size = size;
        entry.mtime_ms = mtime_ms;
        entry.stage = stage;

        entries_[entry.path] = std::move(entry);
    }
}

} // namespace light_git
