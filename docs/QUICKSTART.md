# Quickstart Guide

This quickstart teaches you how to initialize a repository, configure your identity, stage files, make commits, inspect history, create branches, and navigate changes with `lgit`.

---

## 1. Configure Your Identity

Before creating commits, configure your name and email. You can configure them globally (for all repositories) or locally (for the current repository):

```bash
# Set globally in ~/.light-gitconfig
lgit config --global user.name "Your Name"
lgit config --global user.email "you@example.com"
```

Verify your configuration:

```bash
lgit config --list
```

---

## 2. Initialize a Repository

Navigate to your project directory and run:

```bash
mkdir my-project
cd my-project
lgit init
```

This creates the `.light-git/` directory structure with `main` as the default branch.

---

## 3. Create Files and Check Status

Create some project files:

```bash
echo "# My Project" > README.md
mkdir src
echo "int main() { return 0; }" > src/main.cpp
```

Check the repository status:

```bash
lgit status
```

You will see:

```text
light-git  v0.1.0
lightweight version control

branch    main
status    2 untracked

Untracked files:
  (use 'lgit add <file>...' to include in what will be committed)
  ?  README.md
  ?  src/main.cpp
```

---

## 4. Staging Changes

Stage all untracked files:

```bash
lgit add .
```

Or stage a specific file:

```bash
lgit add README.md
```

Check status again:

```bash
lgit status
```

Notice files are now listed under **Changes to be committed**.

---

## 5. Creating Your First Commit

Commit staged files:

```bash
lgit commit -m "feat: initial commit with README and main entry"
```

Output:

```text
[main a1b2c3d] feat: initial commit with README and main entry

 2 files changed, 2 insertions(+), 0 deletions(-)

commit created successfully
```

---

## 6. Inspecting History

View your commit history:

```bash
lgit log
```

Or view a concise graph summary:

```bash
lgit log --oneline
```

---

## 7. Working with Branches

Create and switch to a new branch:

```bash
lgit switch -c feature/auth
```

Make changes on your branch:

```bash
echo "// Auth module" > src/auth.cpp
lgit add src/auth.cpp
lgit commit -m "feat: add auth module"
```

List branches:

```bash
lgit branch
```

Switch back to `main`:

```bash
lgit switch main
```

Notice that `src/auth.cpp` is removed from your working directory while on `main`, and reappears when you switch back to `feature/auth`.

---

## 8. Inspecting and Restoring Changes

If you modify a file:

```bash
echo "Modified content" >> README.md
lgit diff
```

Discard unstaged working directory changes:

```bash
lgit restore README.md
```

If you staged changes and want to unstage them without modifying your file:

```bash
lgit add README.md
lgit restore --staged README.md
```

---

## 9. Cleaning Up and Verification

Verify repository database integrity:

```bash
lgit fsck
```

Run garbage collection to purge unreferenced objects:

```bash
lgit gc
```
