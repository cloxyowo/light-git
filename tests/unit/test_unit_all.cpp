#include "../test_framework.hpp"
#include "light_git/crypto.hpp"
#include "light_git/ignore.hpp"
#include "light_git/diff.hpp"
#include "light_git/config.hpp"
#include "light_git/tree.hpp"
#include "light_git/commit.hpp"
#include "light_git/index.hpp"
#include "light_git/object_store.hpp"
#include "light_git/filesystem.hpp"
#include <filesystem>
#include <fstream>

using namespace light_git;

TEST(Crypto_Sha256_StandardVectors) {
    // 1. Empty string
    ObjectId id1 = hash_string("");
    ASSERT_EQ(id1.to_hex(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // 2. "abc"
    ObjectId id2 = hash_string("abc");
    ASSERT_EQ(id2.to_hex(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // 3. Multi-block string
    ObjectId id3 = hash_string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    ASSERT_EQ(id3.to_hex(), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Crypto_ObjectId_Conversions) {
    std::string hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    ObjectId id = ObjectId::from_hex(hex);
    ASSERT_EQ(id.to_hex(), hex);
    ASSERT_EQ(id.to_short_hex(7), "ba7816b");
    ASSERT_FALSE(id.is_zero());

    ObjectId zero_id;
    ASSERT_TRUE(zero_id.is_zero());
    ASSERT_THROWS(ObjectId::from_hex("invalid_length"));
}

TEST(Ignore_RulesMatching) {
    IgnoreMatcher matcher;
    matcher.load_from_string(
        "# Comments\n"
        "*.o\n"
        "!important.o\n"
        "/build/\n"
        "docs/**/temp.txt\n"
        "target\n"
    );

    // *.o matches files
    ASSERT_TRUE(matcher.is_ignored("main.o", false));
    ASSERT_TRUE(matcher.is_ignored("src/utils.o", false));

    // !important.o negates
    ASSERT_FALSE(matcher.is_ignored("important.o", false));

    // /build/ only matches directory at root
    ASSERT_TRUE(matcher.is_ignored("build", true));
    ASSERT_FALSE(matcher.is_ignored("sub/build", true));

    // docs/**/temp.txt
    ASSERT_TRUE(matcher.is_ignored("docs/temp.txt", false));
    ASSERT_TRUE(matcher.is_ignored("docs/a/b/temp.txt", false));
    ASSERT_FALSE(matcher.is_ignored("other/temp.txt", false));

    // target matches target anywhere
    ASSERT_TRUE(matcher.is_ignored("target", false));
    ASSERT_TRUE(matcher.is_ignored("sub/target", false));
    ASSERT_FALSE(matcher.is_ignored("targeted.cpp", false));
}

TEST(Diff_IdenticalStrings) {
    std::string text = "Line 1\nLine 2\nLine 3\n";
    FileDiff diff = DiffEngine::compute_diff(text, text, "file.txt", "file.txt");
    ASSERT_TRUE(diff.empty());
    ASSERT_EQ(diff.insertions, 0);
    ASSERT_EQ(diff.deletions, 0);
}

TEST(Diff_SingleModification) {
    std::string old_t = "A\nB\nC\n";
    std::string new_t = "A\nB modified\nC\n";
    FileDiff diff = DiffEngine::compute_diff(old_t, new_t, "test.txt", "test.txt");
    ASSERT_FALSE(diff.empty());
    ASSERT_EQ(diff.insertions, 1);
    ASSERT_EQ(diff.deletions, 1);
    ASSERT_EQ(diff.hunks.size(), 1);
}

TEST(Config_ParsingAndTypes) {
    Config cfg;
    std::string ini = 
        "[user]\n"
        "\tname = Alice\n"
        "\temail = alice@example.com\n"
        "[core]\n"
        "\tignorecase = true\n"
        "\tcount = 42\n";

    std::filesystem::path temp_cfg = "temp_test_config.ini";
    std::ofstream ofs(temp_cfg);
    ofs << ini;
    ofs.close();

    cfg.load_file(temp_cfg);
    std::filesystem::remove(temp_cfg);

    ASSERT_EQ(cfg.get_or("user.name", ""), "Alice");
    ASSERT_EQ(cfg.get_or("user.email", ""), "alice@example.com");
    ASSERT_TRUE(cfg.get_bool("core.ignorecase"));
    ASSERT_EQ(cfg.get_int("core.count"), 42);
}

TEST(Objects_TreeSerialization) {
    Tree tree;
    ObjectId id1 = hash_string("hello");
    ObjectId id2 = hash_string("world");

    tree.add_entry({0100644, ObjectType::Blob, "hello.txt", id1});
    tree.add_entry({0100755, ObjectType::Blob, "script.sh", id2});

    auto serialized = tree.serialize();
    Tree deserialized = Tree::deserialize(serialized);

    ASSERT_EQ(deserialized.entries().size(), 2);
    const auto* e1 = deserialized.find_entry("hello.txt");
    ASSERT_NE(e1, nullptr);
    ASSERT_EQ(e1->id, id1);
    ASSERT_EQ(e1->mode, 0100644);

    const auto* e2 = deserialized.find_entry("script.sh");
    ASSERT_NE(e2, nullptr);
    ASSERT_EQ(e2->id, id2);
    ASSERT_EQ(e2->mode, 0100755);
}

TEST(Objects_CommitSerialization) {
    Commit commit;
    commit.tree_id = hash_string("tree_data");
    commit.parents = {hash_string("parent_commit")};
    commit.author = {"Test Author", "test@example.com", 1600000000, "+0200"};
    commit.committer = commit.author;
    commit.message = "Initial commit\n\nDetailed message body.";

    auto serialized = commit.serialize();
    Commit deserialized = Commit::deserialize(serialized);

    ASSERT_EQ(deserialized.tree_id, commit.tree_id);
    ASSERT_EQ(deserialized.parents.size(), 1);
    ASSERT_EQ(deserialized.parents[0], commit.parents[0]);
    ASSERT_EQ(deserialized.author.name, "Test Author");
    ASSERT_EQ(deserialized.author.email, "test@example.com");
    ASSERT_EQ(deserialized.author.timestamp, 1600000000);
    ASSERT_EQ(deserialized.message, commit.message);
}

TEST(Index_SerializationAndIntegrity) {
    Index idx;
    ObjectId b1 = hash_string("content 1");
    ObjectId b2 = hash_string("content 2");

    idx.add_or_update({"src/main.cpp", b1, 0100644, 120, 1000, 0});
    idx.add_or_update({"README.md", b2, 0100644, 45, 1001, 0});

    std::filesystem::path idx_path = "temp_test_index.idx";
    idx.save_to_file(idx_path);

    Index loaded_idx;
    loaded_idx.load_from_file(idx_path);

    ASSERT_EQ(loaded_idx.size(), 2);
    ASSERT_NE(loaded_idx.find("src/main.cpp"), nullptr);
    ASSERT_EQ(loaded_idx.find("src/main.cpp")->id, b1);
    ASSERT_NE(loaded_idx.find("README.md"), nullptr);
    ASSERT_EQ(loaded_idx.find("README.md")->id, b2);

    // Corrupt index checksum test
    std::vector<uint8_t> corrupted_data = fs_util::read_file_bytes(idx_path);
    corrupted_data[15] ^= 0xFF; // Modify a byte
    fs_util::write_file_bytes(idx_path, corrupted_data.data(), corrupted_data.size());

    Index corrupt_idx;
    ASSERT_THROWS(corrupt_idx.load_from_file(idx_path));

    std::filesystem::remove(idx_path);
}

int main() {
    return test_framework::run_all_tests();
}
