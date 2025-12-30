#!/bin/bash
# NetProbe Installation Script
# Usage: curl -fsSL https://raw.githubusercontent.com/Dsuwita/netProbe/main/install.sh | bash

set -e

VERSION="1.0.0"
REPO="Dsuwita/netProbe"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
MAN_DIR="${MAN_DIR:-/usr/local/share/man/man1}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;36m'
NC='\033[0m' # No Color

info() {
    echo -e "${BLUE}==>${NC} $1"
}

success() {
    echo -e "${GREEN}✓${NC} $1"
}

error() {
    echo -e "${RED}✗${NC} $1" >&2
}

warning() {
    echo -e "${YELLOW}!${NC} $1"
}

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
    warning "Running as root. This is not recommended."
fi

# Detect OS and architecture
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
    Linux*)
        OS="linux"
        ;;
    Darwin*)
        OS="darwin"
        ;;
    *)
        error "Unsupported operating system: $OS"
        exit 1
        ;;
esac

case "$ARCH" in
    x86_64|amd64)
        ARCH="x86_64"
        ;;
    aarch64|arm64)
        ARCH="arm64"
        ;;
    *)
        error "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

info "NetProbe v${VERSION} Installer"
info "OS: $OS | Architecture: $ARCH"
echo

# Check for required commands
if ! command -v curl >/dev/null 2>&1; then
    error "curl is required but not installed"
    exit 1
fi

# Create temporary directory
TMP_DIR=$(mktemp -d)
trap "rm -rf $TMP_DIR" EXIT

info "Downloading NetProbe binary..."
BINARY_URL="https://github.com/${REPO}/releases/download/v${VERSION}/netprobe-${OS}-${ARCH}"

if ! curl -fsSL "$BINARY_URL" -o "$TMP_DIR/netprobe" 2>/dev/null; then
    warning "Pre-built binary not available for $OS-$ARCH"
    info "Building from source instead..."
    
    # Check for build dependencies
    if ! command -v cmake >/dev/null 2>&1; then
        error "cmake is required to build from source"
        exit 1
    fi
    
    if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
        error "g++ or clang++ is required to build from source"
        exit 1
    fi
    
    # Clone and build
    info "Cloning repository..."
    cd "$TMP_DIR"
    git clone "https://github.com/${REPO}.git" netprobe-src
    cd netprobe-src
    
    info "Building NetProbe..."
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    cp netprobe "$TMP_DIR/netprobe"
    cp ../man/netprobe.1 "$TMP_DIR/netprobe.1"
else
    success "Binary downloaded"
    
    # Download man page
    info "Downloading man page..."
    curl -fsSL "https://raw.githubusercontent.com/${REPO}/main/man/netprobe.1" \
        -o "$TMP_DIR/netprobe.1"
fi

# Make binary executable
chmod +x "$TMP_DIR/netprobe"

# Install binary
info "Installing binary to $INSTALL_DIR..."
if [ -w "$INSTALL_DIR" ]; then
    cp "$TMP_DIR/netprobe" "$INSTALL_DIR/netprobe"
else
    sudo cp "$TMP_DIR/netprobe" "$INSTALL_DIR/netprobe"
fi
success "Binary installed"

# Install man page
if [ -f "$TMP_DIR/netprobe.1" ]; then
    info "Installing man page to $MAN_DIR..."
    if [ -w "$MAN_DIR" ]; then
        cp "$TMP_DIR/netprobe.1" "$MAN_DIR/netprobe.1"
    else
        sudo mkdir -p "$MAN_DIR"
        sudo cp "$TMP_DIR/netprobe.1" "$MAN_DIR/netprobe.1"
    fi
    success "Man page installed"
fi

# Set capabilities for raw sockets (Linux only)
if [ "$OS" = "linux" ]; then
    info "Setting network capabilities..."
    if command -v setcap >/dev/null 2>&1; then
        if sudo setcap cap_net_raw+ep "$INSTALL_DIR/netprobe" 2>/dev/null; then
            success "Network capabilities set (ping/trace/sniff will work without sudo)"
        else
            warning "Failed to set capabilities. You may need sudo for ping/trace/sniff commands"
        fi
    else
        warning "setcap not found. You may need sudo for ping/trace/sniff commands"
    fi
fi

echo
success "NetProbe installed successfully!"
echo
info "Quick Start:"
echo "  netprobe --help                    # Show all commands"
echo "  netprobe ping google.com           # Ping a host"
echo "  netprobe scan localhost 1-1024     # Scan ports"
echo "  netprobe bench httpbin.org/get 5s  # HTTP benchmark"
echo
info "Documentation: man netprobe"
info "Repository: https://github.com/${REPO}"
