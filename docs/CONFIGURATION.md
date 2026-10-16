# Configuration Reference

`light-git` uses a simple, INI-style configuration format with sections and key-value pairs.

---

## 1. Configuration Levels and Priority

1. **Local Repository Configuration**:
   - Location: `.light-git/config`
   - Applies only to the current repository.
   - Takes precedence over global settings.

2. **Global User Configuration**:
   - Location:
     - Windows: `%USERPROFILE%\.light-gitconfig`
     - Linux / macOS: `$HOME/.light-gitconfig`
   - Applies to all repositories created or accessed by the current user.

---

## 2. Configuration Format

```ini
[user]
    name = Alice Developer
    email = alice@example.com

[core]
    bare = false
    ignorecase = true

[ui]
    color = true

[init]
    defaultBranch = main
```

---

## 3. Supported Configuration Keys

| Key | Default | Description |
| --- | ------- | ----------- |
| `user.name` | *(None)* | Author and committer name recorded in new commits. |
| `user.email` | *(None)* | Author and committer email recorded in new commits. |
| `core.bare` | `false` | Indicates whether the repository is a bare repository. |
| `core.ignorecase` | `true` | Controls case sensitivity for filesystem path comparisons. |
| `ui.color` | `true` | Enables or disables terminal ANSI coloring. |
| `init.defaultBranch` | `main` | Default branch name used when running `lgit init`. |

---

## 4. Configuration Commands

### Setting a Setting Locally
```bash
lgit config user.name "Alice Developer"
lgit config user.email "alice@example.com"
```

### Setting a Setting Globally
```bash
lgit config --global user.name "Alice Developer"
lgit config --global user.email "alice@example.com"
```

### Reading a Specific Key
```bash
lgit config user.name
```

### Listing All Active Settings
```bash
lgit config --list
```
Displays merged local and global configurations.
