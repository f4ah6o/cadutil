#!/bin/bash
set -e

# cadutil - CAD Utility CLI Build Script
# Requires: zig (0.14+), rust/cargo

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Building cadutil (CAD Utility CLI) ==="

# Check dependencies
if ! command -v zig &> /dev/null; then
    echo "Error: zig is not installed"
    echo "Install from: https://ziglang.org/download/"
    exit 1
fi

if ! command -v cargo &> /dev/null; then
    echo "Error: cargo (Rust) is not installed"
    echo "Install from: https://rustup.rs/"
    exit 1
fi

echo "zig version: $(zig version)"
echo "cargo version: $(cargo --version)"
echo

# Build C++ core library
echo "=== Building C++ core (cadutil_core) ==="
cd core
zig build --release=safe
cd ..

echo "Library built: core/zig-out/lib/librecad_core.so"
echo

# Build Rust CLI
echo "=== Building Rust CLI (cadutil) ==="
cd cli
cargo build --release
cd ..

echo "CLI built: cli/target/release/cadutil"
echo

# Create symlink for easier access
rm -f cadutil lc
ln -sf cli/target/release/cadutil cadutil

echo "=== Build complete! ==="
echo
echo "Usage:"
echo "  export LD_LIBRARY_PATH=$SCRIPT_DIR/core/zig-out/lib:\$LD_LIBRARY_PATH"
echo "  export RUST_BACKTRACE=0  # Recommended to avoid libunwind conflicts"
echo "  ./cadutil --help"
echo
echo "Commands:"
echo "  ./cadutil info <file.dxf>       - Show file information"
echo "  ./cadutil validate <file.dxf>   - Validate DXF file"
echo "  ./cadutil convert <in> <out>    - Convert between formats"
