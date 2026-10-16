#pragma once

#include "light_git/types.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace light_git {

class Sha256Hasher {
public:
    Sha256Hasher();
    void update(const void* data, size_t length);
    void update(std::string_view sv);
    ObjectId finalize();

private:
    void transform(const uint8_t block[64]);

    uint32_t state_[8];
    uint64_t count_{0};
    uint8_t buffer_[64]{};
};

ObjectId hash_bytes(const void* data, size_t length);
ObjectId hash_string(std::string_view sv);
ObjectId hash_vector(const std::vector<uint8_t>& data);

} // namespace light_git
