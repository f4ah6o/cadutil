#!/bin/bash
# Build static libraries for all target platforms using Zig cross-compilation
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
CORE_DIR="$ROOT_DIR/src/core"
VENDOR_DIR="$ROOT_DIR/src/cli/vendor"

# Target platforms
TARGETS=(
    "x86_64-linux-gnu"
    "aarch64-linux-gnu"
    "x86_64-macos"
    "aarch64-macos"
    "x86_64-windows-gnu"
)

# Corresponding Rust target triples
RUST_TARGETS=(
    "x86_64-unknown-linux-gnu"
    "aarch64-unknown-linux-gnu"
    "x86_64-apple-darwin"
    "aarch64-apple-darwin"
    "x86_64-pc-windows-gnu"
)

echo "Building static libraries for all platforms..."
echo "Core directory: $CORE_DIR"
echo "Vendor directory: $VENDOR_DIR"
echo ""

cd "$CORE_DIR"

for i in "${!TARGETS[@]}"; do
    ZIG_TARGET="${TARGETS[$i]}"
    RUST_TARGET="${RUST_TARGETS[$i]}"
    
    echo "=== Building for $RUST_TARGET (zig: $ZIG_TARGET) ==="
    
    # Create vendor directory
    mkdir -p "$VENDOR_DIR/$RUST_TARGET"
    
    # Build with Zig
    zig build -Dtarget="$ZIG_TARGET" --release=safe
    
    # Copy library
    if [[ "$ZIG_TARGET" == *"windows"* ]]; then
        cp zig-out/lib/librecad_core.a "$VENDOR_DIR/$RUST_TARGET/recad_core.lib"
    else
        cp zig-out/lib/librecad_core.a "$VENDOR_DIR/$RUST_TARGET/"
    fi
    
    echo "  -> Copied to $VENDOR_DIR/$RUST_TARGET/"
    echo ""
done

echo "All platforms built successfully!"
echo ""
echo "Library sizes:"
find "$VENDOR_DIR" -name "*.a" -o -name "*.lib" | xargs ls -lh
