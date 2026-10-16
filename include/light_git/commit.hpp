#pragma once

#include "light_git/types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace light_git {

struct CommitIdentity {
    std::string name;
    std::string email;
    int64_t timestamp{0};
    std::string timezone{"+0000"};

    std::string format() const;
    static CommitIdentity parse(std::string_view line);
    static CommitIdentity now(std::string name, std::string email);
};

class Commit {
public:
    Commit() = default;

    ObjectId tree_id;
    std::vector<ObjectId> parents;
    CommitIdentity author;
    CommitIdentity committer;
    std::string message;

    std::vector<uint8_t> serialize() const;
    static Commit deserialize(const std::vector<uint8_t>& data);
};

} // namespace light_git
