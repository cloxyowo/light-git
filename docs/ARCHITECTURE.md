# Architecture of light-git

`light-git` (`lgit`) is an independent, lightweight version control system implemented in **ISO C++17**. It does not shell out to or depend on Git in any way, maintaining its own content-addressed storage engine, index format, reference subsystem, and Myers diff implementation.

---

## High-Level Architecture Overview

```
                      +-------------------+
                      |   CLI Dispatcher  |
                      |   (lgit main)     |
                      +---------+---------+
                                |
             +------------------+------------------+
             |                                     |
    +--------v---------+                  +--------v---------+
    |   Command Layer  |                  |   Terminal UI    |
    | (init, status,   |                  | (ANSI colors,    |
    |  commit, diff...) |                  |  box-drawing)    |
    +--------+---------+                  +------------------+
             |
    +--------v-------------------------------------+
    |                 Repository                   |
    |  - Coordinates storage, index & references   |
    |  - Computes status, stages, commits, checks  |
    +--------+------------+------------+-----------+
             |            |            |
+------------v---+  +-----v------+  +--v-------------+
|  Object Store  |  |   Index    |  |   References   |
| (Content-Add.) |  |  (Staging) |  | (HEAD, branch) |
+------------+---+  +------------+  +----------------+
             |
+------------v-------------------------------------+
|           Filesystem & Safety Utilities          |
|  - Atomic replace writes (MoveFileExW / rename)  |
|  - Cryptographic SHA-256 (FIPS 180-4)            |
|  - Lexical path normalization & boundary checks  |
|  - .light-gitignore matching                     |
+--------------------------------------------------+
```

---

## Core Subsystems

### 1. Cryptographic Hash & Identity (`light_git::crypto`, `ObjectId`)
- Every object in light-git is identified by its 256-bit SHA-256 hash.
- Implemented natively without OpenSSL or third-party crypto libraries to ensure minimal binary size and complete self-containment.
- Validated against NIST standard test vectors.

### 2. Content-Addressed Object Store (`light_git::ObjectStore`)
- Located on disk at `.light-git/objects/xx/yyyy...` (where `xx` is the first 2 hex digits of the SHA-256 hash, and `yyyy...` are the remaining 62 hex characters).
- Storage format:
  ```
  <type> <size_in_bytes>\0<payload>
  ```
- Three core immutable object types:
  - **Blob**: Stores raw file contents.
  - **Tree**: Stores directory snapshots with octal permissions, entry type, name, and 32-byte `ObjectId`.
  - **Commit**: Stores root tree `ObjectId`, parent commit IDs, author/committer identities with timestamps and timezones, and the commit message.
- Deduplication is intrinsic: identical file contents produce identical hashes and are stored exactly once.
- Integrity verification: Every read validates that `SHA-256(raw_bytes) == expected_ObjectId`.

### 3. Binary Staging Index (`light_git::Index`)
- Stored at `.light-git/index`.
- Represents the staging area between the working tree and the committed repository history.
- Format includes an 8-byte magic header (`LGITIDX\1`), version number, entry count, individual entries (path, SHA-256, permissions, file size, modification timestamp), and a trailing 32-byte SHA-256 checksum over the entire file.

### 4. Reference Management (`light_git::References`)
- Manages `HEAD`, branch references (`refs/heads/*`), and tags (`refs/tags/*`).
- Supports symbolic `HEAD` pointing to a branch (`ref: refs/heads/main`) and detached `HEAD` pointing directly to a commit hash.
- Reference writes are atomic to prevent broken pointers during unexpected process termination.

### 5. Diff Engine (`light_git::DiffEngine`)
- Implements the **Myers Diff Algorithm** to find the shortest edit script (SES) between text files.
- Generates standard unified diff hunks with 3 lines of contextual padding.
- Binary files are detected automatically and reported as `Binary files differ` to protect the terminal.

### 6. Terminal UI Formatting (`light_git::term::Terminal`)
- Internal formatting layer providing a purple/violet aesthetic (`ANSI 38;5;141m` / `135m`).
- Auto-detects TTY vs redirected output; respects `NO_COLOR` environment variable and `--no-color` flag.
- Uses Unicode box-drawing characters when supported, with automatic ASCII fallbacks for legacy Windows consoles.
