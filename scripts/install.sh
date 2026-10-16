#!/bin/sh
set -e

# light-git (lgit) Linux / macOS installer

PREFIX="${PREFIX:-$HOME/.local}"
BINDIR="$PREFIX/bin"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

printf "%b\n" "\033[1;35mlight-git (lgit) Unix Installer\033[0m"
printf "%b\n" "==============================="

SOURCE_EXE=""
for candidate in \
    "$PROJECT_DIR/build/Release/lgit" \
    "$PROJECT_DIR/build-release/lgit" \
    "$PROJECT_DIR/build/lgit" \
    "$PROJECT_DIR/bin/lgit"; do
    if [ -f "$candidate" ]; then
        SOURCE_EXE="$candidate"
        break
    fi
done

if [ -z "$SOURCE_EXE" ]; then
    printf "%b\n" "\033[31merror: could not find 'lgit' binary. Please build first: cmake --build build --config Release\033[0m" >&2
    exit 1
fi

printf "%b\n" "Installing lgit to $BINDIR..."
mkdir -p "$BINDIR"
cp "$SOURCE_EXE" "$BINDIR/lgit"
chmod 755 "$BINDIR/lgit"

# Check if BINDIR is in PATH
case ":$PATH:" in
    *":$BINDIR:"*) ;;
    *)
        printf "%b\n" "\033[33mNotice: $BINDIR is not in your PATH.\033[0m"
        printf "%b\n" "Add it by adding the following line to your ~/.bashrc or ~/.zshrc:"
        printf "%b\n" "  export PATH=\"$BINDIR:\$PATH\""
        ;;
esac

printf "%b\n" "\033[32mInstallation complete!\033[0m"
"$BINDIR/lgit" --version
