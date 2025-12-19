# cadutil - CAD Utility CLI

Command-line tools for working with CAD files (DXF/JWW).

> **Note:** This is an unofficial, independent tool not affiliated with any CAD software project.

## Features

- **info**: Display file information (layers, blocks, entities, bounds)
- **validate**: Validate DXF file structure and references
- **convert**: Convert between DXF and JWW formats

## Architecture

```
┌─────────────────────────────────────────┐
│         cadutil (Rust CLI)              │
│  - clap for argument parsing            │
│  - colored output                       │
│  - JSON export                          │
└───────────────┬─────────────────────────┘
                │ FFI (C ABI)
┌───────────────▼─────────────────────────┐
│     cadutil_core.so (C++ / zig)         │
│  - libdxfrw (DXF/DWG parsing)           │
│  - jwwlib (JWW parsing)                 │
└─────────────────────────────────────────┘
```

## Requirements

- **zig** 0.14+ (for C++ compilation)
- **Rust/Cargo** (for CLI)

## Building

```bash
./build.sh
```

Or manually:

```bash
# Build C++ core
cd core
zig build --release=safe

# Build Rust CLI
cd ../cli
cargo build --release
```

## Testing

Run the test suite:

```bash
cd cli
cargo test
```

## Usage

Set the library path first:

```bash
export LD_LIBRARY_PATH=/path/to/cli-tools/core/zig-out/lib:$LD_LIBRARY_PATH
export RUST_BACKTRACE=0  # Recommended
```

### Commands

#### info - Display file information

```bash
# Basic info
cadutil info drawing.dxf

# Detailed info with entities
cadutil info drawing.dxf --detail verbose

# JSON output
cadutil info drawing.dxf --json
```

Detail levels:
- `summary` - File overview only
- `normal` - Layers, blocks, entity counts (default)
- `verbose` - All entities with basic properties
- `full` - Complete entity details

#### validate - Validate a file

```bash
# Human-readable output
cadutil validate drawing.dxf

# JSON output
cadutil validate drawing.dxf --json
```

Checks for:
- Undefined layer references
- Undefined block references
- Invalid geometry (zero radius circles, etc.)
- Missing standard layer "0"

#### convert - Convert between formats

```bash
# DXF to DXF (version conversion)
cadutil convert input.dxf output.dxf --dxf-version 2007

# JWW to DXF
cadutil convert input.jww output.dxf

# DXF to JWW
cadutil convert input.dxf output.jww
```

DXF versions: r12, r14, 2000, 2004, 2007, 2010, 2013, 2018

## Example Output

### info command

```
File Information
================
  File:        drawing.dxf
  Format:      DXF
  DXF Version: AC1015

Statistics
----------
  Layers:   5
  Blocks:   3
  Entities: 1234

Bounding Box
------------
  Min: (0.0000, 0.0000, 0.0000)
  Max: (100.0000, 80.0000, 0.0000)
  Size: 100.0000 x 80.0000

Entity Types
------------
  Line           500
  Circle         100
  Arc             50
  Text            84
  ...
```

### validate command

```
Validation Result
=================
  File: drawing.dxf
  Status: VALID
  Issues: 2

Issues
------
  [WARN] MISSING_LAYER_0: Standard layer '0' not found
  [INFO] EMPTY_DRAWING: Drawing contains no entities
```

## API (C)

The core library provides a C API for integration:

```c
#include "librecad_core.h"

// Open and get info
LcFileInfo* info = lc_get_file_info("drawing.dxf", LC_DETAIL_NORMAL);
printf("Entities: %d\n", info->entity_count);
lc_file_info_free(info);

// Validate
LcValidationResult* result = lc_validate("drawing.dxf");
printf("Valid: %s\n", result->is_valid ? "yes" : "no");
lc_validation_result_free(result);

// Convert
LcError err = lc_convert("input.jww", "output.dxf", LC_DXF_VERSION_2007);
```

## License

GPL-2.0-or-later

## Known Issues

1. **Conversion**: The convert command may not preserve all entities for complex files.

2. **libunwind conflict**: When using with Rust's backtrace feature, there may be conflicts with zig's bundled libunwind. Set `RUST_BACKTRACE=0` to avoid issues.

## File Structure

```
cli-tools/
├── build.sh          # Build script
├── README.md         # This file
├── core/             # C++ core library
│   ├── build.zig     # Zig build configuration
│   ├── include/      # C API headers
│   │   └── librecad_core.h
│   ├── src/          # Implementation
│   │   └── librecad_core.cpp
│   └── zig-out/lib/  # Built libraries
├── cli/              # Rust CLI
│   ├── Cargo.toml
│   ├── build.rs
│   ├── src/
│   │   ├── main.rs
│   │   └── ffi.rs    # FFI bindings
│   └── tests/        # Integration tests
└── test.dxf          # Test file
```
