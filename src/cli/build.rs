use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // .../LibreCAD-cli/cli-tools/cli

    let cli_tools_dir = manifest_dir
        .parent().unwrap() // cli-tools
        .to_path_buf();

    let lib_path = cli_tools_dir.join("core/zig-out/lib");

    println!("cargo:rustc-link-search=native={}", lib_path.display());
    println!("cargo:rustc-link-lib=dylib=recad_core");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_path.display());

    println!(
        "cargo:rerun-if-changed={}",
        lib_path.join("librecad_core.so").display()
    );
}
