# cadutil

Command-line utilities for DXF/JWW conversion, file info, and validation.

This is an unofficial tool and not affiliated with any CAD software project.

## Requirements

- Rust/Cargo
- zig 0.15+ (to build the core library)
- `librecad_core` built from the LibreCAD-cli repository

## Build (from the repo)

```bash
git clone https://github.com/f4ah6o/LibreCAD-cli
cd LibreCAD-cli/cli-tools
./build.sh
```

Or manually:

```bash
cd cli-tools/core
zig build --release=safe
cd ../cli
cargo build --release
```

## Usage

Set the library path first:

```bash
export LD_LIBRARY_PATH=/path/to/LibreCAD-cli/cli-tools/core/zig-out/lib:$LD_LIBRARY_PATH
export RUST_BACKTRACE=0
```

Examples:

```bash
cadutil info drawing.dxf
cadutil info drawing.dxf --detail verbose
cadutil info drawing.dxf --json

cadutil validate drawing.dxf
cadutil validate drawing.dxf --json

cadutil convert input.dxf output.dxf --dxf-version 2007
cadutil convert input.dxf output.jww
```

DXF versions: r12, r14, 2000, 2004, 2007, 2010, 2013, 2018

## Notes

This crate links to the `librecad_core` shared library located under
`cli-tools/core/zig-out/lib`. It is intended to be built inside the
LibreCAD-cli repository; `cargo install` on its own will not work unless
you provide that library.

## License

GPL-2.0-or-later
