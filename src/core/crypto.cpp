#include "light_git/crypto.hpp"
#include <cstring>

namespace light_git {

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) {
    return (e & f) ^ (~e & g);
}

inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) {
    return (a & b) ^ (a & c) ^ (b & c);
}

inline uint32_t sig0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t sig1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t theta0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t theta1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

} // namespace

Sha256Hasher::Sha256Hasher() {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
    count_ = 0;
}

void Sha256Hasher::transform(const uint8_t block[64]) {
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8)  |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (size_t i = 16; i < 64; ++i) {
        w[i] = theta1(w[i - 2]) + w[i - 7] + theta0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (size_t i = 0; i < 64; ++i) {
        uint32_t t1 = h + sig1(e) + choose(e, f, g) + K[i] + w[i];
        uint32_t t2 = sig0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256Hasher::update(const void* data, size_t length) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t index = static_cast<size_t>(count_ & 63);
    count_ += length;

    size_t part_len = 64 - index;
    size_t i = 0;

    if (length >= part_len) {
        std::memcpy(&buffer_[index], p, part_len);
        transform(buffer_);
        for (i = part_len; i + 63 < length; i += 64) {
            transform(&p[i]);
        }
        index = 0;
    }

    if (i < length) {
        std::memcpy(&buffer_[index], &p[i], length - i);
    }
}

void Sha256Hasher::update(std::string_view sv) {
    update(sv.data(), sv.size());
}

ObjectId Sha256Hasher::finalize() {
    uint8_t final_count[8];
    uint64_t bit_count = count_ * 8;
    for (int i = 0; i < 8; ++i) {
        final_count[i] = static_cast<uint8_t>((bit_count >> ((7 - i) * 8)) & 0xFF);
    }

    size_t index = static_cast<size_t>(count_ & 63);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    static const uint8_t padding[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    update(padding, pad_len);
    update(final_count, 8);

    ObjectId id;
    for (size_t i = 0; i < 8; ++i) {
        id.bytes[i * 4 + 0] = static_cast<uint8_t>((state_[i] >> 24) & 0xFF);
        id.bytes[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFF);
        id.bytes[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFF);
        id.bytes[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFF);
    }
    return id;
}

ObjectId hash_bytes(const void* data, size_t length) {
    Sha256Hasher hasher;
    hasher.update(data, length);
    return hasher.finalize();
}

ObjectId hash_string(std::string_view sv) {
    return hash_bytes(sv.data(), sv.size());
}

ObjectId hash_vector(const std::vector<uint8_t>& data) {
    return hash_bytes(data.data(), data.size());
}

} // namespace light_git
