# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security vulnerability in light-git, please do not file a public issue. Instead, report it privately to the maintainers at `security@lightgit.local` or through GitHub Security Advisories.

Please include:
- Description of the vulnerability.
- Reproduction steps or proof of concept.
- Affected platforms and compiler toolchains.

## Security Architecture

- **Path traversal prevention**: All paths staged, restored, or checked out are sanitized and validated against repository boundaries using lexically relative path resolution to prevent directory escape attacks.
- **Object verification**: All objects stored in `.light-git/objects` are verified using cryptographic SHA-256 checksums upon read.
- **Atomic updates**: All reference pointers, index state files, and object files are written to isolated temporary files first and atomically swapped into place.
- **Safe symlink handling**: Symlinks pointing outside the repository tree are never automatically traversed during recursive scanning.
