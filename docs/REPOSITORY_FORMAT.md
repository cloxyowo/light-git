# Repository On-Disk Format

This document specifies the exact layout, file formats, and data serialization used by `light-git` repositories under `.light-git/`.

---

## 1. Directory Structure

A non-bare light-git repository contains a `.light-git/` directory at the working tree root:

```
.light-git/
├── HEAD               # Current branch pointer or detached commit hash
├── config             # Repository-specific INI configuration
├── index              # Binary staging area index file
├── objects/           # Content-addressed immutable object store
│   ├── 0a/            # First 2 hex digits of SHA-256 hash
│   │   └── 3b7c...    # Remaining 62 hex digits (raw object file)
│   └── ...
└── refs/
    ├── heads/         # Local branch references (e.g. main, feature)
    └── tags/          # Tag references
```

In a **bare repository**, the contents of `.light-git/` reside directly in the root directory without a surrounding `.light-git/` folder.

---

## 2. Object Format and Serialization

All objects in `.light-git/objects/` are immutable and identified by the SHA-256 hash of their formatted byte stream.

### Object File Layout

```
[type_string] [size_in_ascii_bytes]\0[raw_payload]
```

- `type_string`: `"blob"`, `"tree"`, `"commit"`, or `"tag"`
- `size_in_ascii_bytes`: Decimal representation of payload size
- `\0`: Single null byte delimiter
- `raw_payload`: Type-specific serialized bytes

### Blob Objects
A blob stores exact raw file contents. The payload is identical to the file bytes on disk.

### Tree Objects
A tree represents a directory snapshot. It contains a list of sorted entries. Each entry is serialized as:

```
<octal_mode> <filename>\0<32_bytes_binary_object_id>
```

- `<octal_mode>`: e.g. `100644` (regular file), `100755` (executable file), `40000` (directory/tree).
- `<filename>`: The filename within the directory (not the full path).
- `\0`: Null byte.
- `<32_bytes_binary_object_id>`: The 32-byte raw binary SHA-256 hash of the target object.

Entries are sorted alphabetically by entry name.

### Commit Objects
A commit records repository history:

```
tree <64_hex_tree_id>
parent <64_hex_parent_id>  (0, 1, or more lines)
author <Name> <<email>> <timestamp_seconds> <timezone_offset>
committer <Name> <<email>> <timestamp_seconds> <timezone_offset>

<commit_message>
```

- `tree`: Root tree object ID.
- `parent`: One line per parent commit. Initial root commits have 0 parent lines.
- `author`: Name, email in angle brackets, UNIX timestamp in seconds, timezone offset (e.g. `+0200`).
- `committer`: Same format as author.
- Blank line separator (`\n`).
- `commit_message`: Freeform text (UTF-8).

---

## 3. Staging Index Format (`.light-git/index`)

The index is stored in an efficient binary format with checksum integrity:

```
+-----------------------------------+
| Magic Header: "LGITIDX\1" (8B)    |
+-----------------------------------+
| Version: uint32 (1)               |
+-----------------------------------+
| Entry Count: uint32               |
+-----------------------------------+
| Entries (repeated Entry Count):   |
|   - Path length: uint16           |
|   - Path: UTF-8 string            |
|   - Object ID: 32 bytes binary    |
|   - Mode: uint32 (e.g. 0100644)   |
|   - File size: uint64             |
|   - Mtime (ms): uint64            |
|   - Stage: uint16                 |
+-----------------------------------+
| SHA-256 Checksum: 32 bytes binary |
+-----------------------------------+
```

- Any modification or truncation in the index produces an immediate checksum mismatch upon loading, preventing data corruption.

---

## 4. References and HEAD

### `HEAD`
Located at `.light-git/HEAD`.
- **Symbolic reference**:
  ```
  ref: refs/heads/main
  ```
- **Detached HEAD**:
  ```
  a1b2c3d4e5f6... (64 hex characters)
  ```

### Branch References
Located under `.light-git/refs/heads/<branch_name>`. Contains the 64-character hex SHA-256 hash of the tip commit followed by a newline:
```
af6ed498e465315b62e68387f0b0189ed5c15dcbc7baede23016265e2dec99bb
```

---

## 5. Atomic Guarantees

Every write to the index, reference pointers, configuration, or object files is written to a uniquely named temporary file (`<path>.tmp.<rand>`) in the target directory and replaced using native atomic OS primitives:
- Windows: `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`
- POSIX: `rename(...)`

This guarantees that power outages or process terminations never leave half-written objects or corrupted references.
