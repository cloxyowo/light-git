# Changelog

All notable changes to **light-git** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2026-10-16

### Added
- **Remote Synchronization (`lgit remote`, `lgit fetch`, `lgit push`, `lgit pull`)**:
  - Remote tracking configuration (`remote add`, `remove`, `get-url`, `set-url`, `list` / `-v`).
  - Remote tracking branch references (`refs/remotes/<remote>/<branch>`).
  - Object and commit graph replication between repositories with SHA-256 verification.
  - Fast-forward validation with BFS ancestry traversal.
  - Support for `--force` flag on `lgit push`.
  - Automatic fetch and 3-way merge integration on `lgit pull`.
- **Repository Cloning (`lgit clone`)**:
  - Clone repositories from filesystem directories, `.lgitpack` binary archives, or remote Git URLs (e.g. GitHub/GitLab).
  - Transparent Git bridge automatically imports tracked files, sets `origin` remote, and initializes `.light-git`.
  - Added shallow clone support with `--depth <n>`.
  - Automatic fallback to `.gitignore` when `.light-gitignore` is not present, with layered rule merging.
  - Configurable branch selection with `-b <branch>`.
  - Automatic working tree materialization, staging index initialization, and `origin` remote setup.
- **Portable Binary Bundles (`lgit bundle` / `.lgitpack`)**:
  - Zero-dependency portable binary archive format (`LGITPACK` v1) with trailer SHA-256 checksums.
  - Air-gapped backup and repository transfer without network infrastructure.
  - Subcommands: `create`, `verify`, `unbundle`, and `list-heads`.
- **Multi-Working Tree Subsystem (`lgit worktree`)**:
  - Linked working trees sharing common object store and references while maintaining isolated `HEAD` and staging indexes.
  - Subcommands: `add <path> [<branch>]`, `list`, `remove <path> [--force]`, and `prune`.
  - Automatic gitdir/commondir pointer management.
- **Engine Hardening & Safe Revision Syntax**:
  - Support for relative revision selectors: `HEAD~N`, `HEAD~`, and `HEAD^`.
  - Strict error handling across GC reachability, tree decoding, and ref resolution.
  - Stash commit preservation during garbage collection.
- **Automated Integration Tests**:
  - Comprehensive integration test cases for Remotes & Sync, Bundles & Clones, and Linked Worktrees.

## [0.2.0] - 2026-10-16

### Added
- **Tagging Subsystem (`lgit tag`)**:
  - Full lightweight and reference tagging pointing to commits (`.light-git/refs/tags/`).
  - Commands to create, list, and delete tags (`-d`).
  - Integration with revision resolver, `lgit show`, and `lgit log`.
- **Workspace Stash Stack (`lgit stash`)**:
  - Shelves dirty working directory and staging index modifications to an isolated stash commit.
  - Reset working tree safely to clean HEAD on `stash push`.
  - Commands: `push` (with custom messages), `list`, `pop`, `apply`, `drop`, and `clear`.
- **Three-Way Merge Engine (`lgit merge`)**:
  - Lowest common ancestor (merge base) detection via commit graph traversal.
  - Automatic fast-forward detection when branch tip is ancestor.
  - Complete 3-way file-level and line-level merge with conflict marker generation (`<<<<<<< HEAD` / `=======` / `>>>>>>>`).
  - Automatic two-parent merge commit creation on clean merges.
- **History Cherry-Picking (`lgit cherry-pick`)**:
  - Selectively applies diffs from specified commits onto the current branch tip.
  - Preserves original author attribution and commit summary.
- **Git Interoperability Bridge (`lgit export-git`)**:
  - Full conversion of `.light-git` history into standard Git fast-import format.
  - Automatic initialization and streaming to `.git` repositories to push to GitHub remotes.
- **Automated Test Coverage**:
  - Added end-to-end integration tests for Tag, Stash, Merge, Cherry-pick, and Git Export workflows.

## [0.1.0] - 2026-10-16

### Initial Release

#### Core Version Control Engine
- Independent content-addressed object store under `.light-git/objects/`.
- FIPS 180-4 compliant SHA-256 cryptographic hashing implementation.
- Deduplicated immutable Blob, Tree, and Commit object storage.
- Atomic file operations for index, references, and objects.
- Binary staging index (`.light-git/index`) with magic header and SHA-256 checksum integrity verification.
- Reference management supporting symbolic HEAD and detached HEAD states.
- Myers diff algorithm implementation for unified line-by-line diffing.
- `.light-gitignore` parser supporting wildcards (`*`, `?`), directory patterns, negation (`!`), anchored paths, and `**` directory recursion.
- INI configuration parser with global (`~/.light-gitconfig`) and repository-local cascading.

#### Command-Line Interface (`lgit`)
- `init`: Initialize new repository or bare repository with configurable default branch.
- `status`: Modern purple/violet themed status screen showing staged, unstaged, and untracked changes.
- `add`: Add files, directories, `.`, and `-A` to staging index.
- `commit`: Record snapshot with commit statistics, timestamping, and author metadata.
- `log`: Graph-style and `--oneline` commit history viewer.
- `diff`: Line-by-line colored diff for unstaged and staged (`--staged`) changes.
- `branch`: List, create, and delete branches.
- `switch`: Safe branch switching with dirty working directory protection.
- `restore`: Restore working tree files from index or unstage files (`--staged`).
- `reset`: Soft, mixed, and hard reset capabilities.
- `show`: Inspect commit metadata, author details, and associated patch.
- `rm`: Remove tracked files from index and workspace.
- `mv`: Safely move or rename tracked files with index updates.
- `check-ignore`: Diagnostic tool explaining matching ignore rules and line numbers.
- `config`: View, set, and list configuration keys.
- `gc`: Garbage collection marking reachable objects from references and purging orphaned objects.
- `fsck`: Verify repository object store integrity and report corruption.
- `version`: Display version, compiler, and platform build target.
- Explicit roadmap guidance for future commands (`clone`, `fetch`, `pull`, `push`, `remote`, `merge`, `stash`, `tag`, `cherry-pick`, `rebase`, `worktree`).

#### Terminal Experience
- Custom terminal formatting layer with ANSI 256-color palette (purple/violet accents).
- Unicode box-drawing support with automatic ASCII fallback for legacy consoles.
- Full compliance with `NO_COLOR` and `--no-color` flags.
- Terminal capability detection (TTY vs piped output).

#### Packaging & Testing
- Automated CTest suite with unit tests and end-to-end integration tests.
- Native Windows PowerShell installer (`scripts/install.ps1`) and uninstaller (`scripts/uninstall.ps1`).
- POSIX shell installer (`scripts/install.sh`) and uninstaller (`scripts/uninstall.sh`).
- Compact release binary size under 400 KB.
