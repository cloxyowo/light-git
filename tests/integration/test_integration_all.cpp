#include "../test_framework.hpp"
#include "light_git/repository.hpp"
#include "light_git/filesystem.hpp"
#include "light_git/terminal.hpp"
#include <filesystem>
#include <fstream>

using namespace light_git;

namespace {
struct Sandbox {
    std::filesystem::path path;
    explicit Sandbox(std::string name) {
        path = std::filesystem::current_path() / ("test_sandbox_" + name);
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path);
    }
    ~Sandbox() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    void write_file(const std::string& rel, const std::string& content) {
        fs_util::write_file_string(path / rel, content);
    }
    std::string read_file(const std::string& rel) {
        return fs_util::read_file_string(path / rel);
    }
};
} // namespace

TEST(Integration_InitAndEmptyStatus) {
    Sandbox sb("init_status");
    Repository repo = Repository::init(sb.path, false, "main");

    ASSERT_TRUE(std::filesystem::exists(sb.path / ".light-git" / "HEAD"));
    ASSERT_TRUE(std::filesystem::exists(sb.path / ".light-git" / "objects"));

    RepoStatus status = repo.compute_status();
    ASSERT_EQ(status.branch_name, "main");
    ASSERT_TRUE(status.is_clean());
    ASSERT_TRUE(status.head_commit.is_zero());
}

TEST(Integration_StagingAndCommitFlow) {
    Sandbox sb("stage_commit");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("file1.txt", "Hello World");
    sb.write_file("sub/file2.txt", "Nested content");
    sb.write_file("folder with space/file3.txt", "Spaces in path");

    RepoStatus s1 = repo.compute_status();
    ASSERT_EQ(s1.untracked.size(), 3);

    // Stage specific path
    repo.stage_path("file1.txt");
    RepoStatus s2 = repo.compute_status();
    ASSERT_EQ(s2.staged_new.size(), 1);
    ASSERT_EQ(s2.untracked.size(), 2);

    // Stage all
    repo.stage_all();
    RepoStatus s3 = repo.compute_status();
    ASSERT_EQ(s3.staged_new.size(), 3);
    ASSERT_EQ(s3.untracked.size(), 0);

    // Create commit
    ObjectId c1 = repo.create_commit("Initial commit with multiple files");
    ASSERT_FALSE(c1.is_zero());
    ASSERT_EQ(repo.refs().resolve_head(), c1);

    RepoStatus s4 = repo.compute_status();
    ASSERT_TRUE(s4.is_clean());
}

TEST(Integration_BranchAndSwitchWorkflow) {
    Sandbox sb("branch_switch");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("code.txt", "version 1 on main");
    repo.stage_all();
    ObjectId c1 = repo.create_commit("commit on main");

    // Create and switch to feature branch
    repo.switch_branch("feature", true);
    ASSERT_EQ(repo.refs().get_current_branch(), "feature");

    // Modify file on feature branch
    sb.write_file("code.txt", "version 2 on feature");
    sb.write_file("new_feature.txt", "brand new feature file");
    repo.stage_all();
    ObjectId c2 = repo.create_commit("commit on feature");
    ASSERT_NE(c1, c2);

    // Switch back to main
    repo.switch_branch("main", false);
    ASSERT_EQ(repo.refs().get_current_branch(), "main");
    ASSERT_EQ(sb.read_file("code.txt"), "version 1 on main");
    ASSERT_FALSE(std::filesystem::exists(sb.path / "new_feature.txt"));

    // Switch back to feature
    repo.switch_branch("feature", false);
    ASSERT_EQ(repo.refs().get_current_branch(), "feature");
    ASSERT_EQ(sb.read_file("code.txt"), "version 2 on feature");
    ASSERT_TRUE(std::filesystem::exists(sb.path / "new_feature.txt"));
}

TEST(Integration_RestoreAndReset) {
    Sandbox sb("restore_reset");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("doc.txt", "Original content");
    repo.stage_all();
    ObjectId c1 = repo.create_commit("First commit");

    // Modify file in working tree
    sb.write_file("doc.txt", "Modified content in working tree");
    RepoStatus s1 = repo.compute_status();
    ASSERT_EQ(s1.unstaged_modified.size(), 1);

    // Restore working file
    repo.restore_file("doc.txt", false);
    ASSERT_EQ(sb.read_file("doc.txt"), "Original content");

    // Stage changes and unstage via restore --staged
    sb.write_file("doc.txt", "New staged change with updated content");
    repo.stage_all();
    RepoStatus s2 = repo.compute_status();
    ASSERT_EQ(s2.staged_modified.size(), 1);

    repo.restore_file("doc.txt", true); // Unstage
    RepoStatus s3 = repo.compute_status();
    ASSERT_EQ(s3.staged_modified.size(), 0);
    ASSERT_EQ(s3.unstaged_modified.size(), 1);

    // Commit second version
    repo.stage_all();
    ObjectId c2 = repo.create_commit("Second commit");

    // Hard reset back to c1
    repo.reset(c1, ResetMode::Hard);
    ASSERT_EQ(repo.refs().resolve_head(), c1);
    ASSERT_EQ(sb.read_file("doc.txt"), "Original content");
}

TEST(Integration_MoveAndRemove) {
    Sandbox sb("mv_rm");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("old_name.txt", "persistent text");
    repo.stage_all();
    repo.create_commit("Add old_name");

    // Move
    repo.move_path("old_name.txt", "new_name.txt");
    ASSERT_FALSE(std::filesystem::exists(sb.path / "old_name.txt"));
    ASSERT_TRUE(std::filesystem::exists(sb.path / "new_name.txt"));
    ASSERT_EQ(sb.read_file("new_name.txt"), "persistent text");

    repo.create_commit("Rename to new_name");

    // Remove
    repo.remove_path("new_name.txt", false);
    ASSERT_FALSE(std::filesystem::exists(sb.path / "new_name.txt"));
    repo.create_commit("Delete new_name");
    RepoStatus s = repo.compute_status();
    ASSERT_TRUE(s.is_clean());
}

TEST(Integration_FsckAndGarbageCollection) {
    Sandbox sb("fsck_gc");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("keep.txt", "keep me");
    repo.stage_all();
    ObjectId c1 = repo.create_commit("Initial commit");

    // Fsck on healthy repo
    auto issues = repo.verify_integrity();
    ASSERT_EQ(issues.size(), 0);

    // Create an unreferenced blob directly in object store (unreachable object)
    ObjectId unreachable = repo.objects().write_object(ObjectType::Blob, "unreachable data");
    ASSERT_TRUE(repo.objects().has_object(unreachable));

    // Garbage collect should remove it
    auto gc_res = repo.garbage_collect();
    ASSERT_TRUE(gc_res.removed_objects >= 1);
    ASSERT_FALSE(repo.objects().has_object(unreachable));
    ASSERT_TRUE(repo.objects().has_object(c1)); // Live commit kept!

    // Verify fsck detects corruption
    // Intentionally corrupt an object file
    std::string hex = c1.to_hex();
    std::filesystem::path obj_path = repo.git_dir() / "objects" / hex.substr(0, 2) / hex.substr(2);
    std::vector<uint8_t> corrupted = fs_util::read_file_bytes(obj_path);
    corrupted.back() ^= 0xFF;
    fs_util::write_file_bytes(obj_path, corrupted.data(), corrupted.size());

    auto corrupt_issues = repo.verify_integrity();
    ASSERT_TRUE(!corrupt_issues.empty());
}

int main() {
    return test_framework::run_all_tests();
}
