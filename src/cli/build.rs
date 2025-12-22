use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let target = env::var("TARGET").unwrap_or_else(|_| "x86_64-unknown-linux-gnu".to_string());

    // When building from the repository (not from crates.io), use the local build
    let local_lib_path = manifest_dir
        .parent()
        .unwrap()
        .join("core")
        .join("zig-out")
        .join("lib");

    // When installed from crates.io, use bundled vendor libraries
    let vendor_lib_path = manifest_dir.join("vendor").join(&target);

    // Prefer vendor (for crates.io installs), fallback to local build
    let lib_path = if vendor_lib_path.exists() {
        vendor_lib_path
    } else if local_lib_path.exists() {
        // Local development: use dynamic linking
        println!("cargo:rustc-link-search=native={}", local_lib_path.display());
        println!("cargo:rustc-link-lib=dylib=recad_core");
        println!(
            "cargo:rustc-link-arg=-Wl,-rpath,{}",
            local_lib_path.display()
        );
        println!(
            "cargo:rerun-if-changed={}",
            local_lib_path.join("librecad_core.so").display()
        );
        return;
    } else {
        panic!(
            "Could not find librecad_core library.\n\
             For local development: run 'cd ../core && zig build'\n\
             For crates.io: vendor libraries should be bundled in vendor/{target}/"
        );
    };

    // Static linking for crates.io distribution
    println!("cargo:rustc-link-search=native={}", lib_path.display());
    println!("cargo:rustc-link-lib=static=recad_core");

    // Link C++ standard library - Zig uses LLVM's libc++
    if target.contains("apple") {
        println!("cargo:rustc-link-lib=c++");
    } else if target.contains("linux") {
        // Zig uses libc++ (LLVM), not libstdc++ (GNU)
        println!("cargo:rustc-link-lib=c++");
        println!("cargo:rustc-link-lib=c++abi");
    } else if target.contains("windows") {
        // Windows with GNU toolchain uses libc++ when built with Zig
        println!("cargo:rustc-link-lib=c++");
    }

    println!(
        "cargo:rerun-if-changed={}",
        lib_path.join("librecad_core.a").display()
    );
}
