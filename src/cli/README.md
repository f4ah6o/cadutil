# cadutil

Command-line utilities for DXF/JWW conversion, file info, and validation.

This is an unofficial tool and not affiliated with any CAD software project.

## Installation

### From crates.io

```bash
# Linux: install libc++ first
sudo apt-get install libc++-dev libc++abi-dev  # Debian/Ubuntu

cargo install cadutil
```

### From source (development)

```bash
git clone https://github.com/f4ah6o/cadutil
cd cadutil/src/core
zig build --release=safe
cd ../cli
cargo build --release
```

## Usage

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

## Supported Platforms

| Platform | Architecture |
|----------|-------------|
| Linux | x86_64, aarch64 |
| macOS | x86_64, aarch64 |
| Windows | x86_64 |

## Requirements

### For crates.io installation
- **Linux**: `libc++` and `libc++abi` (installed via package manager)
- **macOS**: No additional dependencies
- **Windows**: No additional dependencies

### For development
- Rust/Cargo
- Zig 0.13+

## License

GPL-2.0-or-later
