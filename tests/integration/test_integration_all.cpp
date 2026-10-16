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

TEST(Integration_TaggingWorkflow) {
    Sandbox sb("tagging");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("version.txt", "1.0.0");
    repo.stage_all();
    ObjectId c1 = repo.create_commit("Release 1.0.0");

    repo.create_tag("v1.0.0", c1);
    auto tags = repo.list_tags();
    ASSERT_EQ(tags.size(), 1);
    ASSERT_EQ(tags[0], "v1.0.0");

    auto resolved = repo.resolve_revision("v1.0.0");
    ASSERT_TRUE(resolved.has_value());
    ASSERT_EQ(*resolved, c1);

    repo.delete_tag("v1.0.0");
    ASSERT_EQ(repo.list_tags().size(), 0);
}

TEST(Integration_StashWorkflow) {
    Sandbox sb("stash");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("work.txt", "Original text\n");
    repo.stage_all();
    repo.create_commit("Base commit");

    // Dirty working directory
    sb.write_file("work.txt", "Modified text\n");
    sb.write_file("new_staged.txt", "new staged\n");
    repo.stage_path("new_staged.txt");

    RepoStatus s_dirty = repo.compute_status();
    ASSERT_FALSE(s_dirty.is_clean());

    ObjectId stash_id = repo.stash_push("WIP on work");
    ASSERT_TRUE(stash_id);

    RepoStatus s_clean = repo.compute_status();
    ASSERT_TRUE(s_clean.is_clean());
    ASSERT_EQ(sb.read_file("work.txt"), "Original text\n");
    ASSERT_FALSE(std::filesystem::exists(sb.path / "new_staged.txt"));

    auto stashes = repo.list_stashes();
    ASSERT_EQ(stashes.size(), 1);
    ASSERT_EQ(stashes[0].message, "WIP on work");

    // Pop stash
    repo.stash_pop(0);
    ASSERT_EQ(sb.read_file("work.txt"), "Modified text\n");
    ASSERT_TRUE(std::filesystem::exists(sb.path / "new_staged.txt"));
    ASSERT_EQ(sb.read_file("new_staged.txt"), "new staged\n");
    ASSERT_EQ(repo.list_stashes().size(), 0);
}

TEST(Integration_MergeWorkflow) {
    Sandbox sb("merge");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("base.txt", "Base content\n");
    repo.stage_all();
    repo.create_commit("Initial commit");

    // Create feature branch
    repo.create_branch("feature");
    repo.switch_branch("feature");
    sb.write_file("feature.txt", "Feature content\n");
    repo.stage_all();
    repo.create_commit("Feature commit");

    // Switch back to main and make another commit
    repo.switch_branch("main");
    sb.write_file("main_only.txt", "Main branch content\n");
    repo.stage_all();
    repo.create_commit("Main commit");

    // Merge feature into main
    auto mres = repo.merge("feature");
    ASSERT_FALSE(mres.has_conflicts);
    ASSERT_TRUE(mres.commit_id);

    // Both files should now exist
    ASSERT_TRUE(std::filesystem::exists(sb.path / "feature.txt"));
    ASSERT_TRUE(std::filesystem::exists(sb.path / "main_only.txt"));

    // Verify merge commit has 2 parents
    RawObject cobj = repo.objects().read_object(mres.commit_id);
    Commit mc = Commit::deserialize(cobj.data);
    ASSERT_EQ(mc.parents.size(), 2);
}

TEST(Integration_CherryPickAndExportGit) {
    Sandbox sb("cherry_export");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("app.txt", "Version 1\n");
    repo.stage_all();
    repo.create_commit("Initial app");

    // Create a feature branch with a cherry-pickable commit
    repo.create_branch("hotfix");
    repo.switch_branch("hotfix");
    sb.write_file("patch.txt", "Security patch applied\n");
    repo.stage_all();
    ObjectId fix_cid = repo.create_commit("Apply security patch");

    // Switch back to main
    repo.switch_branch("main");
    ASSERT_FALSE(std::filesystem::exists(sb.path / "patch.txt"));

    // Cherry pick
    ObjectId new_cid = repo.cherry_pick(fix_cid);
    ASSERT_TRUE(new_cid);
    ASSERT_TRUE(std::filesystem::exists(sb.path / "patch.txt"));
    ASSERT_EQ(sb.read_file("patch.txt"), "Security patch applied\n");

    // Test Git fast-import exporter
    std::string fastimport = repo.export_git_fastimport();
    ASSERT_TRUE(fastimport.find("commit refs/heads/main") != std::string::npos);
    ASSERT_TRUE(fastimport.find("patch.txt") != std::string::npos);
    ASSERT_TRUE(fastimport.find("Security patch applied") != std::string::npos);
}

TEST(Integration_RevisionTraversalAndGcStash) {
    Sandbox sb("rev_gc_stash");
    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("file.txt", "v1\n");
    repo.stage_all();
    ObjectId c1 = repo.create_commit("commit 1");

    sb.write_file("file.txt", "v2\n");
    repo.stage_all();
    ObjectId c2 = repo.create_commit("commit 2");

    // 1. Revision resolution: HEAD~1, HEAD~, HEAD^
    auto r_tilde1 = repo.resolve_revision("HEAD~1");
    auto r_tilde = repo.resolve_revision("HEAD~");
    auto r_caret = repo.resolve_revision("HEAD^");
    auto r_head = repo.resolve_revision("HEAD");

    ASSERT_TRUE(r_head);
    ASSERT_EQ(*r_head, c2);
    ASSERT_TRUE(r_tilde1);
    ASSERT_EQ(*r_tilde1, c1);
    ASSERT_TRUE(r_tilde);
    ASSERT_EQ(*r_tilde, c1);
    ASSERT_TRUE(r_caret);
    ASSERT_EQ(*r_caret, c1);

    // 2. Commit diffing
    auto diffs = repo.diff_commits(c1, c2);
    ASSERT_EQ(diffs.size(), 1);
    ASSERT_EQ(diffs[0].new_path, "file.txt");

    // 3. Stash changes and ensure GC does NOT purge stash objects
    sb.write_file("file.txt", "v3 stashed changes\n");
    ObjectId stash_cid = repo.stash_push("WIP on file");
    ASSERT_TRUE(stash_cid);

    // Run garbage collection
    auto gc_res = repo.garbage_collect();
    ASSERT_TRUE(gc_res.kept_objects > 0);

    // Verify stash still exists and can be popped without missing object error
    auto stashes = repo.list_stashes();
    ASSERT_EQ(stashes.size(), 1);
    repo.stash_pop(0);
    ASSERT_EQ(sb.read_file("file.txt"), "v3 stashed changes\n");
}

TEST(Integration_RemoteAndSyncWorkflow) {
    Sandbox sb_local("sync_local");
    Sandbox sb_remote("sync_remote");

    // Init remote repository as bare repo
    Repository repo_remote = Repository::init(sb_remote.path, true, "main");

    // Init local repository
    Repository repo_local = Repository::init(sb_local.path, false, "main");
    repo_local.config().set_local("user.name", "Tester");
    repo_local.config().set_local("user.email", "tester@lightgit.local");

    // Remote operations: add, get-url, list
    repo_local.add_remote("origin", sb_remote.path.string());
    auto remotes = repo_local.list_remotes();
    ASSERT_EQ(remotes.size(), 1);
    ASSERT_EQ(remotes[0].name, "origin");
    ASSERT_EQ(repo_local.get_remote_url("origin"), sb_remote.path.string());

    // Create commit on local and push
    sb_local.write_file("greeting.txt", "Hello from local\n");
    repo_local.stage_all();
    ObjectId c1 = repo_local.create_commit("Initial local commit");

    auto push_res = repo_local.push_remote("origin", "main");
    ASSERT_EQ(push_res.results.size(), 1);
    ASSERT_TRUE(push_res.results[0].is_new);
    ASSERT_EQ(push_res.results[0].new_commit, c1);

    // Verify remote has the commit
    auto r_cid = repo_remote.refs().get_branch_commit("main");
    ASSERT_TRUE(r_cid);
    ASSERT_EQ(*r_cid, c1);

    // Push again: should report everything up to date
    auto push_res2 = repo_local.push_remote("origin", "main");
    ASSERT_EQ(push_res2.results[0].old_commit, push_res2.results[0].new_commit);

    // Remove remote
    repo_local.remove_remote("origin");
    ASSERT_EQ(repo_local.list_remotes().size(), 0);
}

TEST(Integration_BundleAndCloneWorkflow) {
    Sandbox sb_src("bundle_src");
    Sandbox sb_dst("bundle_dst");
    Sandbox sb_clone("clone_dir");

    Repository repo_src = Repository::init(sb_src.path, false, "main");
    repo_src.config().set_local("user.name", "Tester");
    repo_src.config().set_local("user.email", "tester@lightgit.local");

    sb_src.write_file("doc.md", "# Documentation\n");
    repo_src.stage_all();
    ObjectId c1 = repo_src.create_commit("First doc");
    repo_src.create_tag("v1.0.0");

    sb_src.write_file("doc.md", "# Documentation v2\n");
    repo_src.stage_all();
    ObjectId c2 = repo_src.create_commit("Second doc");

    // 1. Create and verify bundle (.lgitpack)
    std::filesystem::path bundle_file = sb_src.path / "repo.lgitpack";
    repo_src.create_bundle(bundle_file);
    ASSERT_TRUE(std::filesystem::exists(bundle_file));

    auto binfo = Repository::verify_bundle(bundle_file);
    ASSERT_TRUE(binfo.object_count > 0);
    ASSERT_TRUE(binfo.references.count("refs/heads/main"));
    ASSERT_EQ(binfo.references["refs/heads/main"], c2);

    // 2. Clone from bundle
    std::filesystem::path clone_bundle_dir = sb_dst.path / "from_bundle";
    Repository repo_cloned_b = Repository::clone_repository(bundle_file.string(), clone_bundle_dir);
    ASSERT_TRUE(std::filesystem::exists(clone_bundle_dir / "doc.md"));
    ASSERT_EQ(repo_cloned_b.refs().resolve_head(), c2);
    ASSERT_TRUE(repo_cloned_b.compute_status().is_clean());

    // 3. Clone from directory repository
    std::filesystem::path clone_repo_dir = sb_clone.path / "from_repo";
    Repository repo_cloned_d = Repository::clone_repository(sb_src.path.string(), clone_repo_dir);
    ASSERT_TRUE(std::filesystem::exists(clone_repo_dir / "doc.md"));
    ASSERT_EQ(repo_cloned_d.refs().resolve_head(), c2);
    ASSERT_TRUE(repo_cloned_d.compute_status().is_clean());
    ASSERT_TRUE(repo_cloned_d.get_remote_url("origin"));
}

TEST(Integration_WorktreeWorkflow) {
    Sandbox sb("worktree_main");
    std::filesystem::path wt_path = sb.path / ".." / "test_sandbox_worktree_linked";
    std::error_code ec;
    std::filesystem::remove_all(wt_path, ec);

    Repository repo = Repository::init(sb.path, false, "main");
    repo.config().set_local("user.name", "Tester");
    repo.config().set_local("user.email", "tester@lightgit.local");

    sb.write_file("shared.txt", "Shared codebase v1\n");
    repo.stage_all();
    ObjectId c1 = repo.create_commit("Initial commit on main");

    // 1. Add linked worktree
    repo.add_worktree(wt_path, "feature-wt");
    ASSERT_TRUE(std::filesystem::exists(wt_path / ".light-git"));
    ASSERT_TRUE(std::filesystem::exists(wt_path / "shared.txt"));

    // 2. List worktrees
    auto wts = repo.list_worktrees();
    ASSERT_EQ(wts.size(), 2);
    ASSERT_TRUE(wts[0].is_main);
    ASSERT_FALSE(wts[1].is_main);
    ASSERT_EQ(wts[1].branch, "feature-wt");

    // 3. Make changes in linked worktree
    Repository wt_repo = Repository::open(wt_path);
    ASSERT_EQ(wt_repo.refs().get_current_branch(), "feature-wt");
    fs_util::write_file_string(wt_path / "wt_only.txt", "Created in worktree\n");
    wt_repo.stage_all();
    ObjectId wt_cid = wt_repo.create_commit("Feature commit in worktree");

    // Main repo branch reflects the worktree commit
    auto br_cid = repo.refs().get_branch_commit("feature-wt");
    ASSERT_TRUE(br_cid);
    ASSERT_EQ(*br_cid, wt_cid);

    // 4. Remove worktree
    repo.remove_worktree(wt_path, true);
    ASSERT_FALSE(std::filesystem::exists(wt_path));

    auto wts_after = repo.list_worktrees();
    ASSERT_EQ(wts_after.size(), 1);

    std::filesystem::remove_all(wt_path, ec);
}

TEST(Integration_CloneGitRepository) {
    std::string check_cmd =
#if defined(_WIN32)
        "git --version > nul 2>&1";
#else
        "git --version > /dev/null 2>&1";
#endif
    if (std::system(check_cmd.c_str()) != 0) {
        // Git is not available in environment; skip test
        return;
    }

    Sandbox sb("clone_git");
    std::filesystem::path git_source = sb.path / "source_git";
    std::filesystem::path lgit_target = sb.path / "target_lgit";
    std::filesystem::create_directories(git_source);

    // Initialize source git repository
    std::string init_cmd = "git -C \"" + git_source.string() + "\" init -b main";
    (void)std::system(init_cmd.c_str());

    std::string cfg1 = "git -C \"" + git_source.string() + "\" config user.name \"Tester\"";
    std::string cfg2 = "git -C \"" + git_source.string() + "\" config user.email \"tester@test.com\"";
    (void)std::system(cfg1.c_str());
    (void)std::system(cfg2.c_str());

    fs_util::write_file_string(git_source / "hello.txt", "Hello from Git repo\n");
    std::string add_cmd = "git -C \"" + git_source.string() + "\" add hello.txt";
    std::string commit_cmd = "git -C \"" + git_source.string() + "\" commit -m \"Initial git commit\"";
    (void)std::system(add_cmd.c_str());
    (void)std::system(commit_cmd.c_str());

    // Clone Git repository using light-git
    Repository repo = Repository::clone_repository(git_source.string(), lgit_target);

    ASSERT_TRUE(std::filesystem::exists(lgit_target / ".light-git"));
    ASSERT_TRUE(std::filesystem::exists(lgit_target / "hello.txt"));
    ASSERT_TRUE(fs_util::read_file_string(lgit_target / "hello.txt").find("Hello from Git repo") != std::string::npos);

    ObjectId head_id = repo.refs().resolve_head();
    ASSERT_TRUE(head_id);

    auto remotes = repo.list_remotes();
    ASSERT_EQ(remotes.size(), 1);
    ASSERT_EQ(remotes[0].name, "origin");
}

int main() {
    return test_framework::run_all_tests();
}

