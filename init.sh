#!/bin/bash
# Coral Development Environment Initialization
# Usage: ./init.sh [--deps-only] [--build-only] [--clean]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Parse arguments
DEPS_ONLY=false
BUILD_ONLY=false
CLEAN=false

for arg in "$@"; do
    case $arg in
        --deps-only) DEPS_ONLY=true ;;
        --build-only) BUILD_ONLY=true ;;
        --clean) CLEAN=true ;;
        --help|-h)
            echo "Coral Development Environment Initialization"
            echo ""
            echo "Usage: ./init.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --deps-only   Only install dependencies, don't build"
            echo "  --build-only  Only build, assume dependencies are installed"
            echo "  --clean       Clean build artifacts before building"
            echo "  --help, -h    Show this help message"
            exit 0
            ;;
        *)
            error "Unknown argument: $arg"
            ;;
    esac
done

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ -f /etc/debian_version ]]; then
        echo "debian"
    elif [[ -f /etc/redhat-release ]]; then
        echo "redhat"
    elif [[ -f /etc/arch-release ]]; then
        echo "arch"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)
info "Detected OS: $OS"

# Install dependencies based on OS
install_deps() {
    info "Installing dependencies..."

    case $OS in
        macos)
            if ! command -v brew &> /dev/null; then
                info "Homebrew not found. Please install it first:"
                echo '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
                exit 1
            fi
            brew install automake libtool boost pkg-config libevent openssl cmake || true
            brew install qt@5 qrencode miniupnpc berkeley-db@4 || true
            ;;
        debian)
            sudo apt-get update
            sudo apt-get install -y \
                build-essential libtool autotools-dev automake pkg-config \
                bsdmainutils python3 libssl-dev libevent-dev \
                libboost-system-dev libboost-filesystem-dev \
                libboost-test-dev libboost-thread-dev \
                libdb-dev libdb++-dev cmake git
            ;;
        redhat)
            sudo dnf install -y \
                gcc-c++ libtool make autoconf automake \
                openssl-devel libevent-devel boost-devel \
                libdb-cxx-devel cmake git
            ;;
        arch)
            sudo pacman -S --needed --noconfirm \
                base-devel boost libevent openssl db cmake git
            ;;
        *)
            warn "Unknown OS - please install dependencies manually"
            warn "Required: autotools, boost, libevent, openssl, berkeley-db, cmake"
            ;;
    esac
}

# Install RandomX library
install_randomx() {
    info "Checking RandomX installation..."

    if [ -f "/usr/local/include/randomx.h" ] || [ -f "/usr/include/randomx.h" ]; then
        info "RandomX already installed"
        return 0
    fi

    info "Building RandomX from source..."

    RANDOMX_DIR=$(mktemp -d)
    cd "$RANDOMX_DIR"

    git clone --depth 1 https://github.com/tevador/RandomX.git
    cd RandomX
    mkdir -p build && cd build
    cmake ..
    make -j$(nproc 2>/dev/null || sysctl -n hw.physicalcpu 2>/dev/null || echo 4)
    sudo make install

    # Update library cache on Linux
    if [ "$OS" = "debian" ] || [ "$OS" = "redhat" ] || [ "$OS" = "arch" ]; then
        sudo ldconfig 2>/dev/null || true
    fi

    cd "$SCRIPT_DIR"
    rm -rf "$RANDOMX_DIR"

    info "RandomX installed successfully"
}

# Clean build artifacts
clean_build() {
    info "Cleaning build artifacts..."
    make clean 2>/dev/null || true
    make distclean 2>/dev/null || true
    rm -f config.status config.log
}

# Build Coral
build_coral() {
    info "Building Coral..."

    # Set up environment for macOS
    if [ "$OS" = "macos" ]; then
        export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
        export LDFLAGS="-L/usr/local/lib -L/opt/homebrew/lib $LDFLAGS"
        export CPPFLAGS="-I/usr/local/include -I/opt/homebrew/include $CPPFLAGS"

        if [ -d "/opt/homebrew/share/aclocal" ]; then
            export ACLOCAL_PATH="/opt/homebrew/share/aclocal:$ACLOCAL_PATH"
        fi
    fi

    # Run autogen if configure doesn't exist
    if [ ! -f "configure" ]; then
        info "Running autogen.sh..."
        ./autogen.sh
    fi

    # Configure
    if [ ! -f "Makefile" ]; then
        info "Configuring build..."
        ./configure CPPFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
    fi

    # Build
    info "Compiling..."
    NPROC=$(nproc 2>/dev/null || sysctl -n hw.physicalcpu 2>/dev/null || echo 4)
    make -j"$NPROC"

    info "Build complete!"
}

# Main execution
main() {
    echo ""
    echo "=========================================="
    echo "    Coral Development Environment Setup"
    echo "=========================================="
    echo ""

    # Verify we're in the right directory
    if [ ! -f "configure.ac" ] || [ ! -f "src/Makefile.am" ]; then
        error "Please run this script from the Coral repository root"
    fi

    if [ "$CLEAN" = true ]; then
        clean_build
    fi

    if [ "$BUILD_ONLY" = false ]; then
        install_deps
        install_randomx
    fi

    if [ "$DEPS_ONLY" = true ]; then
        info "Dependencies installed. Run './init.sh --build-only' to build."
        exit 0
    fi

    build_coral

    echo ""
    echo "=========================================="
    echo "           Setup Complete!"
    echo "=========================================="
    echo ""
    echo "Executables:"
    echo "  Daemon:  ./src/bitcoind"
    echo "  CLI:     ./src/bitcoin-cli"
    echo ""
    echo "Quick start:"
    echo "  ./src/bitcoind -regtest -daemon"
    echo "  ./src/bitcoin-cli -regtest getblockchaininfo"
    echo ""
    echo "For more information, see README.md"
    echo ""
}

main
