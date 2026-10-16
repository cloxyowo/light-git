#include "light_git/types.hpp"
#include <stdexcept>
#include <algorithm>

namespace light_git {

std::string_view to_string(ObjectType type) {
    switch (type) {
        case ObjectType::Blob: return "blob";
        case ObjectType::Tree: return "tree";
        case ObjectType::Commit: return "commit";
        case ObjectType::Tag: return "tag";
        default: return "invalid";
    }
}

ObjectType object_type_from_string(std::string_view str) {
    if (str == "blob") return ObjectType::Blob;
    if (str == "tree") return ObjectType::Tree;
    if (str == "commit") return ObjectType::Commit;
    if (str == "tag") return ObjectType::Tag;
    return ObjectType::Invalid;
}

static inline int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

ObjectId ObjectId::from_hex(std::string_view hex) {
    if (hex.length() != 64) {
        throw std::invalid_argument("Invalid SHA-256 hex string length (must be 64 characters)");
    }
    ObjectId id;
    for (size_t i = 0; i < 32; ++i) {
        int hi = hex_char_to_int(hex[i * 2]);
        int lo = hex_char_to_int(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            throw std::invalid_argument("Invalid character in SHA-256 hex string");
        }
        id.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return id;
}

std::string ObjectId::to_hex() const {
    static constexpr char hex_chars[] = "0123456789abcdef";
    std::string result(64, '0');
    for (size_t i = 0; i < 32; ++i) {
        result[i * 2] = hex_chars[(bytes[i] >> 4) & 0x0F];
        result[i * 2 + 1] = hex_chars[bytes[i] & 0x0F];
    }
    return result;
}

std::string ObjectId::to_short_hex(size_t len) const {
    return to_hex().substr(0, std::min<size_t>(len, 64));
}

bool ObjectId::is_zero() const {
    for (uint8_t b : bytes) {
        if (b != 0) return false;
    }
    return true;
}

} // namespace light_git
