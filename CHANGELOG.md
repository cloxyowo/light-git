# Changelog

All notable changes to **light-git** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
