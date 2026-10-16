# Frequently Asked Questions (FAQ)

### 1. Why is light-git so much smaller than a typical Git installation?
A standard Git installation on Windows or macOS is often between 200 MB and 400 MB because it bundles a complete MSYS2/MinGW Unix emulation runtime, Bash shell, Perl interpreter, Python scripts, cURL, OpenSSL, Tcl/Tk for git-gui, and hundreds of auxiliary executables.

`light-git` (`lgit`) is written directly in pure **C++17** compiled to a single, self-contained native executable (~380 KB) with **zero external runtime dependencies**. It interacts directly with the operating system without any POSIX emulation layer.

### 2. Is light-git compatible with Git repositories?
**No.** `light-git` is an independent version control system with its own repository format stored under `.light-git/` and its own cryptographic data structures (SHA-256 content addressing, independent binary index format).

It does not read or modify `.git` directories and does not attempt to mimic Git's internal packfile formats. A future interoperability bridge or import/export utility is planned on the roadmap, but light-git does not claim Git compatibility.

### 3. Does light-git require Git to be installed?
**No.** `light-git` has zero dependence on Git. It does not shell out to Git, does not require Git in your PATH, and runs completely independently.

### 4. Can light-git clone from or push to GitHub?
Not in the current version (v0.1.0). v0.1.0 focuses on delivering an ultra-fast, ultra-lightweight, rock-solid local version control engine (init, status, add, commit, branch, switch, restore, reset, diff, log, gc, fsck). Remote networking protocols (`clone`, `fetch`, `push`, `pull`) are planned for future roadmap milestones once the core format is battle-tested.

### 5. How does object storage work in light-git?
`light-git` uses immutable content-addressed storage:
- Every file (blob), directory snapshot (tree), and commit is hashed using **SHA-256**.
- The hash determines its storage path in `.light-git/objects/xx/yyyy...`.
- Identical files have identical hashes and are stored only once (automatic deduplication).
- Every object read is cryptographically verified against its hash.

### 6. How do I back up my light-git repository?
Because all history is stored in the self-contained `.light-git` directory:
- You can simply copy or archive the entire repository directory (including `.light-git/`).
- Standard backup tools, cloud storage (OneDrive, Dropbox), or simple zip archives will preserve complete commit history and references.

### 7. What features are planned for future releases?
- Remote operations: `clone`, `fetch`, `pull`, `push`, and `remote` management.
- Merge functionality: Three-way merge engine and conflict markers.
- Stash functionality: `lgit stash` for temporary workspace shelving.
- Tagging: Cryptographic lightweight and annotated tags (`lgit tag`).
- History rewriting tools: `rebase` and `cherry-pick`.
- Git bridge: Importer/exporter to translate commits between `.git` and `.light-git`.
