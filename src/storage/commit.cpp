#include "light_git/commit.hpp"
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <iomanip>

namespace light_git {

std::string CommitIdentity::format() const {
    return name + " <" + email + "> " + std::to_string(timestamp) + " " + timezone;
}

CommitIdentity CommitIdentity::parse(std::string_view line) {
    CommitIdentity id;
    size_t email_start = line.find('<');
    size_t email_end = line.find('>', email_start);
    if (email_start == std::string_view::npos || email_end == std::string_view::npos) {
        throw std::runtime_error("Malformed identity line: " + std::string(line));
    }

    std::string_view name_part = line.substr(0, email_start);
    while (!name_part.empty() && name_part.back() == ' ') {
        name_part.remove_suffix(1);
    }
    id.name = std::string(name_part);
    id.email = std::string(line.substr(email_start + 1, email_end - email_start - 1));

    std::string_view rest = line.substr(email_end + 1);
    while (!rest.empty() && rest.front() == ' ') {
        rest.remove_prefix(1);
    }

    size_t space_pos = rest.find(' ');
    if (space_pos != std::string_view::npos) {
        std::string ts_str(rest.substr(0, space_pos));
        try {
            id.timestamp = std::stoll(ts_str);
        } catch (...) {
            id.timestamp = 0;
        }
        id.timezone = std::string(rest.substr(space_pos + 1));
    } else if (!rest.empty()) {
        try {
            id.timestamp = std::stoll(std::string(rest));
        } catch (...) {
            id.timestamp = 0;
        }
    }
    return id;
}

CommitIdentity CommitIdentity::now(std::string name, std::string email) {
    CommitIdentity id;
    id.name = std::move(name);
    id.email = std::move(email);

    auto now_tp = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now_tp);
    id.timestamp = static_cast<int64_t>(t);

    // Compute timezone offset
    std::tm tm_local{};
    std::tm tm_utc{};
#if defined(_WIN32)
    localtime_s(&tm_local, &t);
    gmtime_s(&tm_utc, &t);
#else
    localtime_r(&t, &tm_local);
    gmtime_r(&t, &tm_utc);
#endif

    std::time_t t_local = std::mktime(&tm_local);
    std::time_t t_utc = std::mktime(&tm_utc);
    long diff_seconds = static_cast<long>(std::difftime(t_local, t_utc));

    char sign = (diff_seconds >= 0) ? '+' : '-';
    if (diff_seconds < 0) diff_seconds = -diff_seconds;
    int hours = diff_seconds / 3600;
    int minutes = (diff_seconds % 3600) / 60;

    std::ostringstream ss;
    ss << sign << std::setfill('0') << std::setw(2) << hours << std::setw(2) << minutes;
    id.timezone = ss.str();

    return id;
}

std::vector<uint8_t> Commit::serialize() const {
    std::ostringstream ss;
    ss << "tree " << tree_id.to_hex() << "\n";
    for (const auto& parent : parents) {
        ss << "parent " << parent.to_hex() << "\n";
    }
    ss << "author " << author.format() << "\n";
    ss << "committer " << committer.format() << "\n";
    ss << "\n";
    ss << message;

    std::string str = ss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

Commit Commit::deserialize(const std::vector<uint8_t>& data) {
    Commit commit;
    std::string_view sv(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream((std::string(sv)));
    std::string line;

    bool reading_headers = true;
    while (std::getline(stream, line)) {
        // Strip trailing \r if any
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (reading_headers) {
            if (line.empty()) {
                reading_headers = false;
                continue;
            }
            if (line.rfind("tree ", 0) == 0) {
                commit.tree_id = ObjectId::from_hex(line.substr(5));
            } else if (line.rfind("parent ", 0) == 0) {
                commit.parents.push_back(ObjectId::from_hex(line.substr(7)));
            } else if (line.rfind("author ", 0) == 0) {
                commit.author = CommitIdentity::parse(line.substr(7));
            } else if (line.rfind("committer ", 0) == 0) {
                commit.committer = CommitIdentity::parse(line.substr(10));
            }
        } else {
            if (!commit.message.empty()) {
                commit.message.push_back('\n');
            }
            commit.message.append(line);
        }
    }

    if (commit.tree_id.is_zero()) {
        throw std::runtime_error("Commit missing valid root tree ID");
    }

    return commit;
}

} // namespace light_git
