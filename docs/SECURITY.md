# Security Architecture and Model

`light-git` treats version control data as high-value user assets. This document outlines the security architecture, integrity mechanisms, filesystem protections, and threat mitigations implemented across the engine.

---

## 1. Cryptographic Data Integrity

- **Cryptographic Object Identification**: Every object (Blob, Tree, Commit) is addressed exclusively by the 256-bit SHA-256 hash of its serialized byte stream.
- **Verification on Read**: Whenever `lgit` reads an object from disk (`.light-git/objects/xx/yyyy...`), it recomputes the SHA-256 hash of the content before parsing. If the computed hash differs from the requested identifier, an immediate integrity exception is raised, preventing the use of corrupt or tampered data.
- **Repository Verification (`lgit fsck`)**: The `fsck` command iterates through all stored objects, verifies their hashes, validates tree directory references, checks commit parent ancestry, and confirms that all branch and tag references point to authentic commits.

---

## 2. Filesystem Protections

### Directory Traversal & Path Escape Prevention
- `light-git` strictly sanitizes all incoming paths from user CLI arguments, tree objects, and index entries.
- Paths are normalized to eliminate redundant `.` and `..` segments using `fs_util::normalize_path()`.
- Every path target is verified using `fs_util::to_repo_relative()`. Any attempt to stage, restore, or write a path resolving outside the repository root is rejected immediately with an error.

### Atomic Updates & Crash Consistency
- Traditional version control tools can suffer from index or ref corruption if killed mid-write.
- `light-git` uses an atomic staging technique:
  1. The new payload is written to a unique temporary file (`<target>.tmp.<rand>`).
  2. The file is flushed to disk.
  3. The target file is atomically replaced:
     - On Windows: `MoveFileExW(temp, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`
     - On POSIX: `rename(temp, target)`
- If a write is interrupted by a system crash or kill signal, the original repository files remain untouched.

### Symlink Handling
- `light-git` never traverses symlinks outside the repository boundary during recursive directory iterations.
- Regular file operations will not overwrite targets outside repository roots through dangling symlinks.

### Binary File Detection
- To protect terminal emulators from binary garbage or escape sequences, `light-git` inspects file payloads for null bytes (`\0`) within the first 8,192 bytes.
- Binary files are marked as `Binary files differ` in diffs.

---

## 3. Safe Garbage Collection (`lgit gc`)

- Garbage collection executes a strict **mark-and-sweep** algorithm:
  1. **Mark Phase**: Traverses all branch references (`refs/heads/*`), tag references (`refs/tags/*`), detached `HEAD`, and currently staged index entries. Commits, tree hierarchies, and blob objects reachable from these roots are marked.
  2. **Sweep Phase**: Only objects unreferenced by any live reference or index are deleted. Live history is never pruned.

---

## 4. Known Security Limitations

- **No Remote Encryption**: light-git v0.1.0 operates locally. Future networking layers (`push`/`pull`) will implement TLS 1.3 encryption.
- **No File Permission Emulation on Windows**: On Windows, Windows ACLs are used; POSIX execution bits (`0100755`) are preserved in repository trees for cross-platform portability.
