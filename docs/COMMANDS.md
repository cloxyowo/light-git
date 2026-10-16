# Command Reference

Comprehensive documentation for all `light-git` (`lgit`) commands.

---

## Global Options

- `--version`, `-v`: Display version, compiler, and target architecture.
- `--help`, `-h`: Show general help or command-specific documentation.
- `--no-color`: Disable all ANSI color formatting (respects `NO_COLOR` environment variable).

---

## Core Implemented Commands

### `lgit init`

Initialize a new light-git repository.

**Usage:**
```bash
lgit init [<directory>] [--bare]
```

**Options:**
- `directory`: Optional directory to initialize in (defaults to current working directory).
- `--bare`: Create a bare repository without a working tree.

**Exit Codes:**
- `0`: Success
- `1`: Directory creation failure or invalid path

---

### `lgit status`

Show working tree status: current branch, staged changes, unstaged changes, and untracked files.

**Usage:**
```bash
lgit status
```

**Exit Codes:**
- `0`: Success
- `1`: Not a light-git repository

---

### `lgit add`

Stage file modifications, additions, and deletions into the index.

**Usage:**
```bash
lgit add <path>...
lgit add .
lgit add -A
```

**Options:**
- `-A`, `--all`: Stage all changes (new, modified, and deleted files) in the working tree.
- `.`: Stage all changes in the current directory subtree.

**Exit Codes:**
- `0`: Success
- `1`: Missing pathspec or path outside repository

---

### `lgit commit`

Record changes from the index to the repository history.

**Usage:**
```bash
lgit commit -m "<message>" [--amend]
```

**Options:**
- `-m <message>`: Commit message (required).
- `--amend`: Replace the tip of the current branch by creating a new commit sharing the current commit's parent.

**Exit Codes:**
- `0`: Success
- `1`: Empty message, author identity missing, or no staged changes

---

### `lgit log`

Display commit history starting from HEAD.

**Usage:**
```bash
lgit log [--oneline] [-n <count>]
```

**Options:**
- `--oneline`: Compact one-line graph representation showing abbreviated hash, branch reference, and first line of commit message.
- `-n <count>`: Limit number of commits displayed.

---

### `lgit diff`

Show differences between files in the working directory, the index, and commits.

**Usage:**
```bash
lgit diff
lgit diff --staged
```

**Options:**
- `--staged`, `--cached`: View differences between the staging index and the HEAD commit. Without this flag, shows differences between working tree and index.

---

### `lgit branch`

List, create, or delete branches.

**Usage:**
```bash
lgit branch                     # List all local branches
lgit branch <branch_name>       # Create new branch at current HEAD
lgit branch -d <branch_name>    # Delete branch
```

**Options:**
- `-d <branch_name>`, `-D <branch_name>`: Delete specified branch.

---

### `lgit switch`

Switch branches safely without overwriting uncommitted work.

**Usage:**
```bash
lgit switch <branch>
lgit switch -c <new_branch>
```

**Options:**
- `-c <new_branch>`: Create and immediately switch to a new branch.

---

### `lgit restore`

Restore working tree files or unstage files from the index.

**Usage:**
```bash
lgit restore <path>...          # Revert working tree files to index state
lgit restore --staged <path>... # Unstage files from index (reverts index to HEAD)
```

---

### `lgit reset`

Reset the current HEAD, index, and working tree to a specified state.

**Usage:**
```bash
lgit reset [--soft | --mixed | --hard] [<commit>]
```

**Modes:**
- `--mixed` (default): Moves HEAD pointer to target commit and resets the index to match target commit. Working tree is preserved.
- `--soft`: Moves HEAD pointer only. Index and working tree are preserved.
- `--hard`: Moves HEAD pointer, resets index, and overwrites working tree files to match target commit.

---

### `lgit show`

Display commit metadata, author, date, message, and unified patch diff.

**Usage:**
```bash
lgit show [<commit>]
```

---

### `lgit rm`

Remove tracked files from the index and optionally the working tree.

**Usage:**
```bash
lgit rm <path>...
lgit rm --cached <path>...
```

**Options:**
- `--cached`: Remove file from index only (untrack), keeping it on disk.

---

### `lgit mv`

Move or rename a tracked file while automatically updating the index.

**Usage:**
```bash
lgit mv <source> <destination>
```

---

### `lgit check-ignore`

Check whether a path matches `.light-gitignore` rules and display the matching pattern and file location.

**Usage:**
```bash
lgit check-ignore <path>...
```

**Exit Codes:**
- `0`: At least one specified path is ignored
- `1`: None of the paths are ignored

---

### `lgit config`

Get and set repository-local or global user options.

**Usage:**
```bash
lgit config <key>                     # Read value
lgit config <key> <value>             # Set local repository value
lgit config --global <key> <value>    # Set global user value
lgit config --list                    # List all active options
```

---

### `lgit gc`

Perform garbage collection to purge unreachable objects from `.light-git/objects`.

**Usage:**
```bash
lgit gc
```

---

### `lgit fsck`

Verify the cryptographic integrity of all objects, trees, commit references, and head ancestry.

**Usage:**
```bash
lgit fsck
```

**Exit Codes:**
- `0`: Integrity check passed
- `1`: Corrupt objects or broken references detected

---

### `lgit tag`

Create, list, or delete tag references pointing to commits.

**Usage:**
```bash
lgit tag                        # List all tags in alphabetical order
lgit tag <name>                 # Create tag at current HEAD
lgit tag <name> <commit>        # Create tag at specific commit
lgit tag -d <name>              # Delete a tag
```

---

### `lgit stash`

Save dirty working tree and staging area changes away to a stash stack, resetting to clean HEAD.

**Usage:**
```bash
lgit stash                      # Save dirty changes
lgit stash push -m "message"    # Save dirty changes with description
lgit stash list                 # List all stashed entries
lgit stash pop [index]          # Apply and remove stash (default stash@{0})
lgit stash apply [index]        # Apply without removing from stack
lgit stash drop [index]         # Drop a specific stash entry
lgit stash clear                # Clear all stashed states
```

---

### `lgit merge`

Join two branches together using fast-forward or three-way merge with conflict marker generation.

**Usage:**
```bash
lgit merge <branch>
```

**Behavior:**
- **Fast-forward:** If current branch tip is an ancestor of target, updates branch pointer directly.
- **Three-way merge:** Locates common merge base, reconciles clean file additions/deletions/edits, and automatically generates merge commit.
- **Conflict Handling:** If lines conflict, writes `<<<<<<< HEAD` / `=======` / `>>>>>>>` markers into affected files and sets `.light-git/MERGE_HEAD`.

---

### `lgit cherry-pick`

Apply the changes introduced by an existing commit onto the current branch tip.

**Usage:**
```bash
lgit cherry-pick <commit-hash>
```

---

### `lgit export-git`

Export `.light-git` history into standard Git fast-import stream format or initialize a `.git` repository.

**Usage:**
```bash
lgit export-git                           # Exports and initializes Git repo in ./export_git
lgit export-git --target ./my-git-repo     # Exports to specified target directory
lgit export-git --output history.fi       # Exports raw Git fast-import stream file
```

---

## Planned / Roadmap Commands

The following commands are registered on the light-git future roadmap:

- `lgit clone`: Clone remote or local repository into a new directory.
- `lgit fetch`: Download remote objects and references.
- `lgit pull`: Fetch and integrate remote history.
- `lgit push`: Update remote references with local objects.
- `lgit remote`: Manage tracked remote repositories.
- `lgit rebase`: Reapply commits on top of another base tip (`-i` interactive rebase).
- `lgit worktree`: Manage multiple working trees connected to one repository.
- `lgit ui`: Embedded local web dashboard and visual commit graph viewer.
- `lgit sign`: Cryptographic commit and tag signing via Ed25519.
- `lgit bundle`: Package repository objects into portable `.lgitpack` archives.
- `lgit daemon`: Peer-to-peer and LAN repository discovery and sync daemon.
