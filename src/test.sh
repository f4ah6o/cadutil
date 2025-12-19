#!/bin/bash
set -e

# cadutil Test Script
# Runs unit tests and integration tests

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== cadutil Test Suite ==="
echo

# Set library path
export LD_LIBRARY_PATH="$SCRIPT_DIR/core/zig-out/lib:$LD_LIBRARY_PATH"
export RUST_BACKTRACE=0

# Check if core library exists
if [ ! -f "core/zig-out/lib/librecad_core.so" ]; then
    echo "Error: Core library not found. Run ./build.sh first."
    exit 1
fi

# Run Rust tests
echo "=== Running Unit Tests ==="
cd cli
cargo test --lib -- --nocapture
echo

echo "=== Running Integration Tests ==="
cargo test --test cli_tests -- --nocapture
echo

echo "=== All Tests Passed! ==="
