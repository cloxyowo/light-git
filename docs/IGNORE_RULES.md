# Ignore Rules Reference (`.light-gitignore`)

`light-git` uses `.light-gitignore` files placed in the repository root to exclude untracked files and build artifacts from staging and status checks.

---

## 1. Syntax Rules

### Comments and Blank Lines
- Lines starting with `#` are treated as comments and ignored.
- Blank lines and whitespace-only lines are ignored.

```gitignore
# This is a comment
```

### Wildcards
- `*`: Matches zero or more characters **except** directory separators (`/`).
  - Example: `*.o` matches `main.o` and `src/utils.o`.
- `?`: Matches any single character **except** directory separators (`/`).
  - Example: `temp?.txt` matches `temp1.txt` but not `temp12.txt`.

### Double Asterisks (`**`)
- `**/`: Matches zero or more directories anywhere along the path.
  - Example: `**/logs` matches `logs`, `var/logs`, `a/b/c/logs`.
- `/**`: Matches all files and directories inside.
  - Example: `build/**` matches everything inside the `build` directory.
- `a/**/b`: Matches `a/b`, `a/x/b`, `a/x/y/b`.

### Directory-Only Patterns (`/` at the end)
- Patterns ending with a forward slash `/` match only directories.
  - Example: `temp/` matches directory `temp/` and its contents, but does NOT match a file named `temp`.

### Anchored Patterns (`/` at the beginning)
- Patterns beginning with a forward slash `/` are anchored to the repository root directory.
  - Example: `/build` matches `build/` at the repository root, but does NOT match `src/build/`.

### Negation Patterns (`!`)
- An exclamation mark `!` prefix negates the pattern. If a file was previously ignored by an earlier rule, a matching negation pattern re-includes it.
  - Example:
    ```gitignore
    *.log
    !important.log
    ```
    Matches all `.log` files except `important.log`.

---

## 2. Order of Evaluation

Rules are evaluated from top to bottom. If multiple rules match a path, the **last matching rule wins**.

---

## 3. Debugging Ignore Rules

To diagnose why a file is ignored or verify which rule matches a path, use `lgit check-ignore`:

```bash
lgit check-ignore build/output.bin
```

Output:
```text
.light-gitignore:4:build/   build/output.bin
```

Returns exit code `0` if the file matches an ignore rule, or `1` if the file is not ignored.
