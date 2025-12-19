//! Integration tests for cadutil CLI
//!
//! These tests verify the CLI commands work correctly with real DXF files.

use std::env;
use std::path::PathBuf;
use std::process::Command;

/// Get the path to the test DXF file
fn get_test_dxf_path() -> PathBuf {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set");
    PathBuf::from(manifest_dir).join("..").join("test.dxf")
}

/// Get the cadutil binary path
fn get_binary_path() -> PathBuf {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set");
    PathBuf::from(manifest_dir)
        .join("target")
        .join("debug")
        .join("cadutil")
}

/// Set up the library path environment variable
fn get_lib_path() -> String {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set");
    let lib_path = PathBuf::from(manifest_dir)
        .join("..")
        .join("core")
        .join("zig-out")
        .join("lib");
    lib_path.to_string_lossy().to_string()
}

/// Run cadutil command and return the output
fn run_cadutil(args: &[&str]) -> std::process::Output {
    let binary = get_binary_path();
    let lib_path = get_lib_path();

    Command::new(&binary)
        .args(args)
        .env("LD_LIBRARY_PATH", &lib_path)
        .env("RUST_BACKTRACE", "0")
        .output()
        .expect("Failed to execute cadutil")
}

mod help_tests {
    use super::*;

    #[test]
    fn test_help_command() {
        let output = run_cadutil(&["--help"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Help command should succeed");
        assert!(stdout.contains("cadutil"), "Help should mention cadutil");
        assert!(stdout.contains("CAD file utility"), "Help should contain description");
        assert!(stdout.contains("info"), "Help should list info command");
        assert!(stdout.contains("validate"), "Help should list validate command");
        assert!(stdout.contains("convert"), "Help should list convert command");
    }

    #[test]
    fn test_version_command() {
        let output = run_cadutil(&["version"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Version command should succeed");
        assert!(stdout.contains("cadutil"), "Version should show cadutil");
        assert!(stdout.contains("cadutil_core"), "Version should show core library");
    }

    #[test]
    fn test_info_help() {
        let output = run_cadutil(&["info", "--help"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Info help should succeed");
        assert!(stdout.contains("info"), "Should describe info command");
        assert!(stdout.contains("--detail"), "Should show detail option");
        assert!(stdout.contains("--json"), "Should show json option");
    }

    #[test]
    fn test_validate_help() {
        let output = run_cadutil(&["validate", "--help"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Validate help should succeed");
        assert!(stdout.contains("validate"), "Should describe validate command");
        assert!(stdout.contains("--json"), "Should show json option");
    }

    #[test]
    fn test_convert_help() {
        let output = run_cadutil(&["convert", "--help"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Convert help should succeed");
        assert!(stdout.contains("convert"), "Should describe convert command");
        assert!(stdout.contains("--dxf-version"), "Should show dxf-version option");
    }
}

mod info_tests {
    use super::*;

    #[test]
    fn test_info_basic() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap()]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Info command should succeed");
        assert!(stdout.contains("File Information"), "Should show file information header");
        assert!(stdout.contains("Statistics"), "Should show statistics section");
        assert!(stdout.contains("Layers"), "Should show layers count");
        assert!(stdout.contains("Entities"), "Should show entities count");
    }

    #[test]
    fn test_info_detail_summary() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap(), "--detail", "summary"]);

        assert!(output.status.success(), "Info with summary detail should succeed");
    }

    #[test]
    fn test_info_detail_normal() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap(), "--detail", "normal"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Info with normal detail should succeed");
        assert!(stdout.contains("Layers") || stdout.contains("Layer"), "Should include layer info");
    }

    #[test]
    fn test_info_detail_verbose() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap(), "--detail", "verbose"]);

        assert!(output.status.success(), "Info with verbose detail should succeed");
    }

    #[test]
    fn test_info_detail_full() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap(), "--detail", "full"]);

        assert!(output.status.success(), "Info with full detail should succeed");
    }

    #[test]
    fn test_info_json_output() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap(), "--json"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Info with JSON output should succeed");
        assert!(stdout.contains("{"), "Output should be JSON");
        assert!(stdout.contains("\"filename\""), "JSON should contain filename field");
        assert!(stdout.contains("\"entity_count\""), "JSON should contain entity_count field");
        assert!(stdout.contains("\"layer_count\""), "JSON should contain layer_count field");
    }

    #[test]
    fn test_info_nonexistent_file() {
        let output = run_cadutil(&["info", "/nonexistent/file.dxf"]);

        assert!(!output.status.success(), "Info on nonexistent file should fail");
    }

    #[test]
    fn test_info_invalid_detail_level() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["info", test_file.to_str().unwrap(), "--detail", "invalid"]);

        assert!(!output.status.success(), "Invalid detail level should fail");
    }
}

mod validate_tests {
    use super::*;

    #[test]
    fn test_validate_basic() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["validate", test_file.to_str().unwrap()]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Validate command should succeed");
        assert!(stdout.contains("Validation Result"), "Should show validation result header");
        assert!(stdout.contains("Status"), "Should show status");
    }

    #[test]
    fn test_validate_json_output() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["validate", test_file.to_str().unwrap(), "--json"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Validate with JSON output should succeed");
        assert!(stdout.contains("{"), "Output should be JSON");
        assert!(stdout.contains("\"is_valid\""), "JSON should contain is_valid field");
        assert!(stdout.contains("\"issues\""), "JSON should contain issues field");
    }

    #[test]
    fn test_validate_nonexistent_file() {
        let output = run_cadutil(&["validate", "/nonexistent/file.dxf"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        // Validation should still "succeed" (return result) but show file error as issue
        assert!(stdout.contains("FILE_ERROR") || !output.status.success());
    }
}

mod convert_tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_convert_dxf_to_dxf() {
        let test_file = get_test_dxf_path();
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output.dxf");

        let output = run_cadutil(&[
            "convert",
            test_file.to_str().unwrap(),
            output_file.to_str().unwrap(),
        ]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Convert DXF to DXF should succeed");
        assert!(stdout.contains("Converting") || stdout.contains("completed"));
        assert!(output_file.exists(), "Output file should be created");

        // Verify output file is valid DXF
        let content = fs::read_to_string(&output_file).expect("Failed to read output");
        assert!(content.contains("SECTION"), "Output should be valid DXF");
    }

    #[test]
    fn test_convert_with_version() {
        let test_file = get_test_dxf_path();
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output_r12.dxf");

        let output = run_cadutil(&[
            "convert",
            test_file.to_str().unwrap(),
            output_file.to_str().unwrap(),
            "--dxf-version",
            "r12",
        ]);

        assert!(output.status.success(), "Convert with version should succeed");
        assert!(output_file.exists(), "Output file should be created");
    }

    #[test]
    fn test_convert_version_2007() {
        let test_file = get_test_dxf_path();
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output_2007.dxf");

        let output = run_cadutil(&[
            "convert",
            test_file.to_str().unwrap(),
            output_file.to_str().unwrap(),
            "--dxf-version",
            "2007",
        ]);

        assert!(output.status.success(), "Convert to DXF 2007 should succeed");
        assert!(output_file.exists(), "Output file should be created");
    }

    #[test]
    fn test_convert_version_2018() {
        let test_file = get_test_dxf_path();
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output_2018.dxf");

        let output = run_cadutil(&[
            "convert",
            test_file.to_str().unwrap(),
            output_file.to_str().unwrap(),
            "--dxf-version",
            "2018",
        ]);

        assert!(output.status.success(), "Convert to DXF 2018 should succeed");
        assert!(output_file.exists(), "Output file should be created");
    }

    #[test]
    fn test_convert_invalid_version() {
        let test_file = get_test_dxf_path();
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output.dxf");

        let output = run_cadutil(&[
            "convert",
            test_file.to_str().unwrap(),
            output_file.to_str().unwrap(),
            "--dxf-version",
            "invalid",
        ]);

        assert!(!output.status.success(), "Convert with invalid version should fail");
    }

    #[test]
    fn test_convert_nonexistent_input() {
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output.dxf");

        let output = run_cadutil(&[
            "convert",
            "/nonexistent/file.dxf",
            output_file.to_str().unwrap(),
        ]);

        assert!(!output.status.success(), "Convert nonexistent file should fail");
    }

    #[test]
    fn test_convert_to_jww() {
        let test_file = get_test_dxf_path();
        let temp_dir = tempdir().expect("Failed to create temp dir");
        let output_file = temp_dir.path().join("output.jww");

        let output = run_cadutil(&[
            "convert",
            test_file.to_str().unwrap(),
            output_file.to_str().unwrap(),
        ]);

        assert!(output.status.success(), "Convert DXF to JWW should succeed");
        assert!(output_file.exists(), "JWW output file should be created");
    }
}

mod error_handling_tests {
    use super::*;

    #[test]
    fn test_no_command() {
        let output = run_cadutil(&[]);
        // Should show help or error
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);

        // Either shows help or requires a subcommand
        assert!(
            !output.status.success() || stdout.contains("Usage") || stderr.contains("error"),
            "No command should either fail or show usage"
        );
    }

    #[test]
    fn test_unknown_command() {
        let output = run_cadutil(&["unknown"]);

        assert!(!output.status.success(), "Unknown command should fail");
    }

    #[test]
    fn test_info_missing_file_argument() {
        let output = run_cadutil(&["info"]);

        assert!(!output.status.success(), "Info without file should fail");
    }

    #[test]
    fn test_validate_missing_file_argument() {
        let output = run_cadutil(&["validate"]);

        assert!(!output.status.success(), "Validate without file should fail");
    }

    #[test]
    fn test_convert_missing_arguments() {
        let output = run_cadutil(&["convert"]);

        assert!(!output.status.success(), "Convert without files should fail");
    }

    #[test]
    fn test_convert_missing_output_argument() {
        let test_file = get_test_dxf_path();
        let output = run_cadutil(&["convert", test_file.to_str().unwrap()]);

        assert!(!output.status.success(), "Convert without output file should fail");
    }
}

mod fixture_tests {
    use super::*;

    /// Get path to test fixtures directory
    fn get_fixtures_path() -> PathBuf {
        let manifest_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set");
        PathBuf::from(manifest_dir).join("tests").join("test_fixtures")
    }

    #[test]
    fn test_empty_dxf_info() {
        let empty_file = get_fixtures_path().join("empty.dxf");
        if !empty_file.exists() {
            return; // Skip if fixture doesn't exist
        }

        let output = run_cadutil(&["info", empty_file.to_str().unwrap()]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Info on empty DXF should succeed");
        assert!(stdout.contains("Entities: 0") || stdout.contains("entity_count"),
                "Should show zero entities");
    }

    #[test]
    fn test_empty_dxf_validate() {
        let empty_file = get_fixtures_path().join("empty.dxf");
        if !empty_file.exists() {
            return; // Skip if fixture doesn't exist
        }

        let output = run_cadutil(&["validate", empty_file.to_str().unwrap()]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Validate on empty DXF should succeed");
        // Should have warning about empty drawing
        assert!(stdout.contains("EMPTY_DRAWING") || stdout.contains("no entities"),
                "Should warn about empty drawing");
    }

    #[test]
    fn test_multi_layer_dxf_info() {
        let multi_layer_file = get_fixtures_path().join("multi_layer.dxf");
        if !multi_layer_file.exists() {
            return; // Skip if fixture doesn't exist
        }

        let output = run_cadutil(&["info", multi_layer_file.to_str().unwrap()]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Info on multi-layer DXF should succeed");
        // Should show multiple layers
        assert!(stdout.contains("Walls") || stdout.contains("Layers"),
                "Should show layer information");
    }

    #[test]
    fn test_multi_layer_dxf_json() {
        let multi_layer_file = get_fixtures_path().join("multi_layer.dxf");
        if !multi_layer_file.exists() {
            return; // Skip if fixture doesn't exist
        }

        let output = run_cadutil(&["info", multi_layer_file.to_str().unwrap(), "--json"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "JSON info on multi-layer DXF should succeed");

        // Parse as JSON to validate structure
        let json: serde_json::Value = serde_json::from_str(&stdout)
            .expect("Output should be valid JSON");

        assert!(json["layer_count"].as_i64().unwrap() >= 1, "Should have layers");
        assert!(json["entity_count"].as_i64().unwrap() >= 1, "Should have entities");
    }

    #[test]
    fn test_multi_layer_validate() {
        let multi_layer_file = get_fixtures_path().join("multi_layer.dxf");
        if !multi_layer_file.exists() {
            return; // Skip if fixture doesn't exist
        }

        let output = run_cadutil(&["validate", multi_layer_file.to_str().unwrap(), "--json"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(output.status.success(), "Validate on multi-layer DXF should succeed");

        // Parse as JSON to validate structure
        let json: serde_json::Value = serde_json::from_str(&stdout)
            .expect("Output should be valid JSON");

        assert!(json["is_valid"].as_bool().is_some(), "Should have is_valid field");
        assert!(json["issues"].as_array().is_some(), "Should have issues array");
    }
}
