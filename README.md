```text
                                            
▄▄           ▄▄                             
██ ▀▀        ██     ██             ▀▀  ██   
██ ██  ▄████ ████▄ ▀██▀▀     ▄████ ██ ▀██▀▀ 
██ ██  ██ ██ ██ ██  ██ ▀▀▀▀▀ ██ ██ ██  ██   
██ ██▄ ▀████ ██ ██  ██       ▀████ ██▄ ██   
          ██                    ██          
        ▀▀▀                   ▀▀▀                    
```

# light-git (`lgit`)

[![CI](https://github.com/cloxyowo/light-git/actions/workflows/ci.yml/badge.svg)](https://github.com/cloxyowo/light-git/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/cloxyowo/light-git?color=8b5cf6)](https://github.com/cloxyowo/light-git/releases)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-7c3aed.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Binary Size](https://img.shields.io/badge/Binary-~380_KB-10b981.svg)](docs/BENCHMARKS.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> Ultra-lightweight, high-performance, aesthetically pleasing version control system written entirely in pure **C++17**.

---

> [!WARNING]
> **Independent Repository Format:**  
> `light-git` is an independent version control system that stores history under `.light-git/` using its own content-addressed format and SHA-256 object structures. It is **not** a wrapper around Git and is **not** automatically compatible with `.git` repositories or Git remotes.

---

## Overview

**light-git** is designed for developers who want a fast, minimal, beautiful version control system without the 300+ MB overhead of traditional Git installations.

- **Minimal Footprint:** Native release binary is only **~380 KB** with a total installation footprint under 1 MB.
- **Zero External Dependencies:** Built with pure ISO C++17, native OS APIs, and a custom FIPS 180-4 SHA-256 engine. No MSYS2, no Perl, no Python, no Bash runtime required.
- **Real Version Control Engine:** Complete content-addressed object database (Blobs, Trees, Commits), atomic binary staging index, and reference management.
- **Terminal Aesthetics:** Clean typography with dark terminal-friendly presentation, purple and violet accents, and automatic ASCII fallbacks for legacy consoles.
- **Fast Startup & Operation:** Cold startup executes in **~18 ms**, with commit creation taking under 1 second even across 500 files.

---

## Measured Performance & Footprint

All metrics are measured from Release builds on Windows 10/11 x64 (MSVC 19.51):

| Metric | light-git v0.1.0 | Standard Git (Windows) | Difference |
| ------ | ---------------- | ---------------------- | ---------- |
| **Executable Size** | **386.5 KB** | ~3.5 MB (`git.exe` stub) | **89% smaller** |
| **Installed Footprint** | **< 1.0 MB** | **~280 MB - 350 MB** | **> 99.7% smaller** |
| **Runtime Dependencies** | **0** (Native OS only) | MSYS2, Bash, Perl, cURL, etc. | **Zero dependencies** |
| **Startup Time (`--version`)** | **18.37 ms** | ~40 - 70 ms | **2x - 3x faster** |
| **Init Time (`lgit init`)** | **45.49 ms** | ~60 - 90 ms | Fast |
| **Staging (500 files)** | **2,341 ms** | ~1,800 - 2,500 ms | Comparable |
| **Commit (500 files)** | **916 ms** | ~800 - 1,200 ms | Comparable |
| **Peak Memory Usage** | **< 15 MB** | ~35 - 60 MB | **Low memory** |

*See [docs/BENCHMARKS.md](docs/BENCHMARKS.md) for full benchmark methodology and reproducible measurement scripts.*

---

## Terminal Experience

`light-git` is styled with a restrained, modern aesthetic featuring purple/violet accents:

```text
light-git  v0.1.0
lightweight version control

branch    main
status    2 modified, 1 untracked

Changes to be committed:
  (use 'lgit restore --staged <file>...' to unstage)
  new file:   src/main.cpp

Changes not staged for commit:
  (use 'lgit add <file>...' to update what will be committed)
  (use 'lgit restore <file>...' to discard changes in working directory)
  modified:   src/repository.cpp

Untracked files:
  (use 'lgit add <file>...' to include in what will be committed)
  ?  tests/basic_test.cpp

tip: run 'lgit diff' to inspect your unstaged changes
```

Commit creation summary:

```text
$ lgit commit -m "feat: implement content-addressed object store"

[main a31f9c2] feat: implement content-addressed object store

 4 files changed, 82 insertions(+), 19 deletions(-)

commit created successfully
```

---

## Quickstart

```bash
# 1. Configure author identity
lgit config --global user.name "Alice Developer"
lgit config --global user.email "alice@example.com"

# 2. Initialize a repository
mkdir my-project && cd my-project
lgit init

# 3. Create and stage files
echo "Hello light-git" > README.md
lgit add README.md

# 4. Check status
lgit status

# 5. Commit snapshot
lgit commit -m "feat: initial commit"

# 6. View history
lgit log --oneline

# 7. Create and switch branches
lgit switch -c feature/login
```

*See [docs/QUICKSTART.md](docs/QUICKSTART.md) for a comprehensive introductory walkthrough.*

---

## Command Reference

| Command | Description |
| ------- | ----------- |
| `lgit init` | Initialize a new, empty repository (or `--bare`). |
| `lgit status` | Inspect branch, staged changes, unstaged changes, and untracked files. |
| `lgit add` | Add file contents or directories to the staging index (`.`, `-A`). |
| `lgit commit` | Record staged changes to repository history (`-m`, `--amend`). |
| `lgit log` | Display commit history in graph format or `--oneline`. |
| `lgit diff` | View line-by-line unified diffs (unstaged or `--staged`). |
| `lgit branch` | List, create (`<name>`), or delete (`-d`) branches. |
| `lgit switch` | Safely switch branches (`-c` to create and switch). |
| `lgit restore` | Restore working tree files or unstage files (`--staged`). |
| `lgit reset` | Reset current HEAD and index (`--soft`, `--mixed`, `--hard`). |
| `lgit show` | Display commit details and unified patch diff. |
| `lgit rm` | Remove tracked files from index and working tree (`--cached`). |
| `lgit mv` | Move or rename tracked files and update index. |
| `lgit tag` | Create, list, or delete tags (`-d`, `-a`, `-m`). |
| `lgit stash` | Shelve dirty workspace modifications (`push`, `pop`, `apply`, `list`, `drop`). |
| `lgit merge` | Three-way merge branches with conflict markers (`<<<<<<< HEAD`). |
| `lgit cherry-pick` | Apply the changes introduced by an existing commit. |
| `lgit export-git` | Export repository to Git fast-import format to push to GitHub. |
| `lgit remote` | Manage tracked remotes (`add`, `remove`, `get-url`, `set-url`, `-v`). |
| `lgit fetch` | Download objects and refs into remote tracking branches. |
| `lgit push` | Update remote branch refs with local objects (`--force`). |
| `lgit pull` | Fetch and 3-way merge remote tracking branch into current branch. |
| `lgit clone` | Clone a repository or `.lgitpack` bundle into a new directory. |
| `lgit bundle` | Package and verify portable binary `.lgitpack` archives. |
| `lgit worktree` | Manage multiple concurrent working trees (`add`, `list`, `remove`, `prune`). |
| `lgit check-ignore` | Debug and inspect which `.light-gitignore` rule matches a path. |
| `lgit config` | Read or set configuration options locally or `--global`. |
| `lgit gc` | Clean unreachable objects via mark-and-sweep garbage collection. |
| `lgit fsck` | Verify repository integrity, tree links, and SHA-256 hashes. |
| `lgit version` | Display version and build configuration. |

*See [docs/COMMANDS.md](docs/COMMANDS.md) for complete syntax, flags, and exit codes.*

---

## Instant Installation (One-Liner)

No need to clone or download the full repository. Install `lgit` with a single terminal command:

### Windows (PowerShell)

Open PowerShell and run:

```powershell
irm https://raw.githubusercontent.com/cloxyowo/light-git/main/scripts/install.ps1 | iex
```

*Or from your custom website/host:*
```powershell
irm https://your-website.com/install.ps1 | iex
```

This downloads `lgit.exe`, places it in `%LOCALAPPDATA%\Programs\light-git\bin`, adds it to your User `PATH`, and verifies the installation.

To uninstall at any time:
```powershell
& "$env:LOCALAPPDATA\Programs\light-git\uninstall.ps1"
```

### Linux & macOS (curl)

Open your terminal and run:

```bash
curl -fsSL https://raw.githubusercontent.com/cloxyowo/light-git/main/scripts/install.sh | sh
```

*Or from your custom website/host:*
```bash
curl -fsSL https://your-website.com/install.sh | sh
```

Installs `lgit` directly into `$HOME/.local/bin/lgit` and configures execution permissions.

To uninstall:
```bash
sh ~/.local/uninstall-lgit.sh
```

---

## Local Installation (From Cloned Source)

If you already have the repository cloned:

- **Windows:** `.\scripts\install.ps1`
- **Linux/macOS:** `sh scripts/install.sh`

*See [docs/INSTALLATION.md](docs/INSTALLATION.md) for custom prefixes, self-hosting on your own domain, and manual build options.*

---

## Building from Source

### Requirements
- CMake 3.16+
- C++17 compatible compiler (MSVC 2022+, GCC 9+, Clang 10+)

```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run automated test suite
ctest --test-dir build -C Release --output-on-failure
```

---

## Documentation

Full technical documentation is located in the `docs/` directory:

- [Quickstart Guide](docs/QUICKSTART.md)
- [Command Reference](docs/COMMANDS.md)
- [Architecture & Internal Design](docs/ARCHITECTURE.md)
- [Repository On-Disk Format](docs/REPOSITORY_FORMAT.md)
- [Configuration Reference](docs/CONFIGURATION.md)
- [Ignore Rules & Glob Syntax](docs/IGNORE_RULES.md)
- [Installation Guide](docs/INSTALLATION.md)
- [Security Architecture](docs/SECURITY.md)
- [Performance Benchmarks](docs/BENCHMARKS.md)
- [Troubleshooting Guide](docs/TROUBLESHOOTING.md)
- [Frequently Asked Questions (FAQ)](docs/FAQ.md)

---

## Roadmap

### Milestone 1: Core Content-Addressed VCS Engine (Completed)
- [x] Content-addressed immutable storage (Blobs, Trees, Commits) with SHA-256
- [x] Binary staging index (`.light-git/index`) with checksum verification
- [x] Atomic write safety and rollback across all operations
- [x] Myers line diff algorithm with colored hunks
- [x] `.light-gitignore` pattern matching engine (glob, wildcards, recursion)
- [x] Essential repository commands (`init`, `status`, `add`, `commit`, `log`, `diff`, `branch`, `switch`, `restore`, `reset`, `rm`, `mv`, `show`, `gc`, `fsck`)
- [x] Cross-platform automated installers & uninstallers (PowerShell & POSIX sh)

### Milestone 2: Branching, Tags & Workspaces (Completed)
- [x] Three-way merge engine with automatic conflict markers (`lgit merge`)
- [x] Workspace stash stack (`lgit stash push`, `list`, `pop`, `apply`, `drop`, `clear`)
- [x] Tagging subsystem (`lgit tag` create, list, delete)
- [x] History cherry-picking (`lgit cherry-pick <commit>`)
- [x] Git fast-import bridge (`lgit export-git`) to export repositories and push to GitHub

### Milestone 3: Distributed Collaboration & Networking (Completed)
- [x] Direct repository remote synchronization (`lgit remote`, `lgit push`, `lgit pull`, `lgit fetch`)
- [x] Repository cloning from directory or bundles (`lgit clone`)
- [x] Portable zero-dependency binary archive bundles (`lgit bundle` / `.lgitpack`)
- [x] Multi-working tree support with isolated HEAD and staging index (`lgit worktree`)

### Milestone 4: Cryptographic Security & Developer UX (Next)
- [ ] Native Ed25519 commit and tag signing (`lgit sign`)
- [ ] Interactive terminal rebase (`lgit rebase -i`)
- [ ] Embedded zero-dependency local web dashboard and graph visualizer (`lgit ui`)
- [ ] Staging patch mode (`lgit add -p`)
- [ ] Peer-to-peer / LAN repository discovery daemon (`lgit daemon`)

---

## License

This project is licensed under the [MIT License](LICENSE).
