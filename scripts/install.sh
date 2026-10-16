#!/bin/sh
set -e

# light-git (lgit) Linux / macOS installer
# Supports both local repository install and one-liner web install:
#   curl -fsSL https://your-website.com/install.sh | sh

PREFIX="${PREFIX:-$HOME/.local}"
BINDIR="$PREFIX/bin"
BASE_URL="${BASE_URL:-https://github.com/cloxyowo/light-git/releases/latest/download}"
DOWNLOAD_URL="${DOWNLOAD_URL:-}"

printf "%b\n" "\033[1;35mlight-git (lgit) Unix Installer\033[0m"
printf "%b\n" "==============================="

# Architecture detection
OS="$(uname -s)"
ARCH="$(uname -m)"

SOURCE_EXE=""
if [ -n "$0" ] && [ -f "$0" ]; then
    PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
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
fi

mkdir -p "$BINDIR"

if [ -n "$SOURCE_EXE" ]; then
    printf "%b\n" "Mode: Local Install"
    printf "%b\n" "Source: $SOURCE_EXE"
    cp "$SOURCE_EXE" "$BINDIR/lgit"
else
    printf "%b\n" "Mode: Web Install ($OS $ARCH)"

    if [ -z "$DOWNLOAD_URL" ]; then
        case "$OS" in
            Linux)
                DOWNLOAD_URL="$BASE_URL/lgit-linux-x64"
                ;;
            Darwin)
                case "$ARCH" in
                    arm64|aarch64)
                        DOWNLOAD_URL="$BASE_URL/lgit-macos-arm64"
                        ;;
                    *)
                        DOWNLOAD_URL="$BASE_URL/lgit-macos-x64"
                        ;;
                esac
                ;;
            *)
                DOWNLOAD_URL="$BASE_URL/lgit"
                ;;
        esac
    fi

    printf "%b\n" "Downloading from $DOWNLOAD_URL..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$DOWNLOAD_URL" -o "$BINDIR/lgit"
    elif command -v wget >/dev/null 2>&1; then
        wget -qO "$BINDIR/lgit" "$DOWNLOAD_URL"
    else
        printf "%b\n" "\033[31merror: curl or wget is required to download lgit\033[0m" >&2
        exit 1
    fi
fi

chmod 755 "$BINDIR/lgit"

# Create uninstaller
cat << 'EOF' > "$PREFIX/uninstall-lgit.sh"
#!/bin/sh
set -e
PREFIX="${PREFIX:-$HOME/.local}"
rm -f "$PREFIX/bin/lgit"
printf "%b\n" "\033[32mlight-git uninstalled successfully.\033[0m"
EOF
chmod 755 "$PREFIX/uninstall-lgit.sh"

# PATH check
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
