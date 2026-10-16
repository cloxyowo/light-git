#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <functional>

namespace light_git {

inline constexpr std::string_view LIGHT_GIT_VERSION = "0.2.0";

enum class ObjectType : uint8_t {
    Invalid = 0,
    Blob = 1,
    Tree = 2,
    Commit = 3,
    Tag = 4
};

std::string_view to_string(ObjectType type);
ObjectType object_type_from_string(std::string_view str);

struct ObjectId {
    std::array<uint8_t, 32> bytes{};

    constexpr ObjectId() = default;
    explicit ObjectId(const std::array<uint8_t, 32>& b) : bytes(b) {}

    static ObjectId zero() {
        return ObjectId{};
    }

    static ObjectId from_hex(std::string_view hex);

    [[nodiscard]] std::string to_hex() const;
    [[nodiscard]] std::string to_short_hex(size_t len = 7) const;
    [[nodiscard]] bool is_zero() const;

    bool operator==(const ObjectId& other) const {
        return bytes == other.bytes;
    }

    bool operator!=(const ObjectId& other) const {
        return bytes != other.bytes;
    }

    bool operator<(const ObjectId& other) const {
        return bytes < other.bytes;
    }

    explicit operator bool() const {
        return !is_zero();
    }
};

} // namespace light_git

namespace std {
template <>
struct hash<light_git::ObjectId> {
    size_t operator()(const light_git::ObjectId& id) const noexcept {
        size_t h = 14695981039346656037ULL;
        for (uint8_t b : id.bytes) {
            h ^= static_cast<size_t>(b);
            h *= 1099511628211ULL;
        }
        return h;
    }
};
} // namespace std
