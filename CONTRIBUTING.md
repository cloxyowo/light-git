# Contributing to light-git

Thank you for your interest in contributing to **light-git**!

## Project Principles

1. **Minimal footprint**: We prioritize a compact executable and low resource usage. Avoid adding dependencies.
2. **C++17 strictly**: The codebase strictly uses ISO C++17. Do not introduce C++20 features.
3. **No runtime wrappers**: light-git is an independent version control system, never a wrapper around Git.
4. **Terminal elegance**: Terminal output must follow the purple/violet theme and maintain clean formatting without emojis.
5. **Data safety**: Repositories must never be silently corrupted. Use atomic writes and validate checksums.

## Development Setup

### Prerequisites

- CMake 3.16+
- C++17 compatible compiler:
  - Windows: Visual Studio 2022+ (MSVC 19+)
  - Linux: GCC 9+ or Clang 10+
  - macOS: Apple Clang 12+

### Building

```bash
# Configure build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build binaries
cmake --build build --config Release
```

### Running Tests

```bash
# Run unit and integration test suites via CTest
ctest --test-dir build -C Release --output-on-failure
```

## Pull Request Guidelines

1. Ensure code compiles without warnings (`/W4` on MSVC, `-Wall -Wextra` on GCC/Clang).
2. Write unit and integration tests for every new feature or bugfix.
3. Run the full test suite before submitting.
4. Keep commits clean and descriptive.
