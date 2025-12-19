//! cadutil - Command line tools for DXF/JWW files
//!
//! An unofficial CAD file utility tool for format conversion,
//! file information extraction, and validation.

mod ffi;

use anyhow::Result;
use clap::{Parser, Subcommand};
use colored::*;
use std::path::PathBuf;

use ffi::{LcDetailLevel, LcDxfVersion, LcEntityType, LcSeverity};

#[derive(Parser)]
#[command(name = "cadutil")]
#[command(author = "CAD Utility Contributors")]
#[command(version = env!("CARGO_PKG_VERSION"))]
#[command(about = "CAD file utility - DXF/JWW conversion, info extraction, and validation", long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Convert files between DXF and JWW formats
    Convert {
        /// Input file (DXF or JWW)
        input: PathBuf,

        /// Output file (DXF or JWW)
        output: PathBuf,

        /// DXF version for output (r12, r14, 2000, 2004, 2007, 2010, 2013, 2018)
        #[arg(short = 'V', long, default_value = "2007")]
        dxf_version: String,
    },

    /// Display file information
    Info {
        /// Input file to analyze
        input: PathBuf,

        /// Detail level (summary, normal, verbose, full)
        #[arg(short, long, default_value = "normal")]
        detail: String,

        /// Output as JSON
        #[arg(short, long)]
        json: bool,
    },

    /// Validate a DXF file
    Validate {
        /// Input file to validate
        input: PathBuf,

        /// Output as JSON
        #[arg(short, long)]
        json: bool,
    },

    /// Show library version
    Version,
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Commands::Convert {
            input,
            output,
            dxf_version,
        } => cmd_convert(&input, &output, &dxf_version),

        Commands::Info {
            input,
            detail,
            json,
        } => cmd_info(&input, &detail, json),

        Commands::Validate { input, json } => cmd_validate(&input, json),

        Commands::Version => {
            println!("cadutil {}", env!("CARGO_PKG_VERSION"));
            println!("cadutil_core {}", ffi::version());
            Ok(())
        }
    }
}

fn cmd_convert(input: &PathBuf, output: &PathBuf, dxf_version: &str) -> Result<()> {
    let input_str = input.to_string_lossy();
    let output_str = output.to_string_lossy();

    // Detect formats
    let in_format = ffi::detect_format(&input_str);
    let out_format = ffi::detect_format(&output_str);

    println!(
        "{} {} ({}) -> {} ({})",
        "Converting:".green().bold(),
        input_str,
        in_format.as_str(),
        output_str,
        out_format.as_str()
    );

    // Parse DXF version
    let version: LcDxfVersion = dxf_version
        .parse()
        .map_err(|e: String| anyhow::anyhow!("{}", e))?;

    // Perform conversion
    ffi::convert(&input_str, &output_str, version)
        .map_err(|e| anyhow::anyhow!("Conversion failed: {}", e))?;

    println!("{}", "Conversion completed successfully!".green());
    Ok(())
}

fn cmd_info(input: &PathBuf, detail: &str, json: bool) -> Result<()> {
    let input_str = input.to_string_lossy();

    let detail_level: LcDetailLevel = detail
        .parse()
        .map_err(|e: String| anyhow::anyhow!("{}", e))?;

    if json {
        let json_output = ffi::get_file_info_json(&input_str, detail_level)
            .map_err(|e| anyhow::anyhow!("Failed to get file info: {}", e))?;
        println!("{}", json_output);
    } else {
        let info = ffi::get_file_info(&input_str, detail_level)
            .map_err(|e| anyhow::anyhow!("Failed to get file info: {}", e))?;

        print_file_info(&info, detail_level);
    }

    Ok(())
}

fn print_file_info(info: &ffi::FileInfo, detail: LcDetailLevel) {
    println!("{}", "File Information".cyan().bold());
    println!("{}", "================".cyan());
    println!("  File:        {}", info.filename);
    println!("  Format:      {}", info.format.as_str());
    if !info.dxf_version.is_empty() {
        println!("  DXF Version: {}", info.dxf_version);
    }
    println!();

    println!("{}", "Statistics".cyan().bold());
    println!("{}", "----------".cyan());
    println!("  Layers:   {}", info.layer_count);
    println!("  Blocks:   {}", info.block_count);
    println!("  Entities: {}", info.entity_count);
    println!();

    // Bounds
    let ((min_x, min_y, min_z), (max_x, max_y, max_z)) = info.bounds;
    if min_x <= max_x {
        println!("{}", "Bounding Box".cyan().bold());
        println!("{}", "------------".cyan());
        println!("  Min: ({:.4}, {:.4}, {:.4})", min_x, min_y, min_z);
        println!("  Max: ({:.4}, {:.4}, {:.4})", max_x, max_y, max_z);
        println!(
            "  Size: {:.4} x {:.4}",
            max_x - min_x,
            max_y - min_y
        );
        println!();
    }

    // Entity counts by type
    println!("{}", "Entity Types".cyan().bold());
    println!("{}", "------------".cyan());
    let entity_types = [
        (LcEntityType::Point, "Point"),
        (LcEntityType::Line, "Line"),
        (LcEntityType::Circle, "Circle"),
        (LcEntityType::Arc, "Arc"),
        (LcEntityType::Ellipse, "Ellipse"),
        (LcEntityType::Polyline, "Polyline"),
        (LcEntityType::LwPolyline, "LWPolyline"),
        (LcEntityType::Spline, "Spline"),
        (LcEntityType::Text, "Text"),
        (LcEntityType::MText, "MText"),
        (LcEntityType::Insert, "Insert"),
        (LcEntityType::Hatch, "Hatch"),
        (LcEntityType::Dimension, "Dimension"),
        (LcEntityType::Leader, "Leader"),
        (LcEntityType::Solid, "Solid"),
        (LcEntityType::Image, "Image"),
    ];

    for (et, name) in &entity_types {
        let count = info.entity_counts[*et as usize];
        if count > 0 {
            println!("  {:12} {:>6}", name, count);
        }
    }
    println!();

    // Layer details
    if !info.layers.is_empty() && detail as i32 >= LcDetailLevel::Normal as i32 {
        println!("{}", "Layers".cyan().bold());
        println!("{}", "------".cyan());
        for layer in &info.layers {
            let status = if layer.is_frozen {
                " [frozen]"
            } else if layer.is_off {
                " [off]"
            } else if layer.is_locked {
                " [locked]"
            } else {
                ""
            };
            println!(
                "  {} (color: {}, type: {}){}",
                layer.name, layer.color, layer.line_type, status
            );
        }
        println!();
    }

    // Block details
    if !info.blocks.is_empty() && detail as i32 >= LcDetailLevel::Normal as i32 {
        println!("{}", "Blocks".cyan().bold());
        println!("{}", "------".cyan());
        for block in &info.blocks {
            println!(
                "  {} ({} entities, base: ({:.2}, {:.2}))",
                block.name, block.entity_count, block.base_point.0, block.base_point.1
            );
        }
        println!();
    }

    // Entity details (verbose mode)
    if !info.entities.is_empty() && detail as i32 >= LcDetailLevel::Verbose as i32 {
        println!("{}", "Entities".cyan().bold());
        println!("{}", "--------".cyan());
        let max_show = if detail == LcDetailLevel::Full {
            info.entities.len()
        } else {
            20.min(info.entities.len())
        };

        for (i, entity) in info.entities.iter().take(max_show).enumerate() {
            println!(
                "  {:4}. {:12} layer: {:16} color: {:3}",
                i + 1,
                entity.entity_type.as_str(),
                entity.layer,
                entity.color
            );
        }

        if info.entities.len() > max_show {
            println!(
                "  ... and {} more entities",
                info.entities.len() - max_show
            );
        }
    }
}

fn cmd_validate(input: &PathBuf, json: bool) -> Result<()> {
    let input_str = input.to_string_lossy();

    if json {
        let json_output = ffi::validate_json(&input_str)
            .map_err(|e| anyhow::anyhow!("Validation failed: {}", e))?;
        println!("{}", json_output);
    } else {
        let result = ffi::validate(&input_str)
            .map_err(|e| anyhow::anyhow!("Validation failed: {}", e))?;

        print_validation_result(&result, &input_str);
    }

    Ok(())
}

fn print_validation_result(result: &ffi::ValidationResult, filename: &str) {
    println!("{}", "Validation Result".cyan().bold());
    println!("{}", "=================".cyan());
    println!("  File: {}", filename);

    if result.is_valid {
        println!("  Status: {}", "VALID".green().bold());
    } else {
        println!("  Status: {}", "INVALID".red().bold());
    }

    println!("  Issues: {}", result.issues.len());
    println!();

    if !result.issues.is_empty() {
        println!("{}", "Issues".cyan().bold());
        println!("{}", "------".cyan());

        for issue in &result.issues {
            let severity_str = match issue.severity {
                LcSeverity::Error => "ERROR".red().bold(),
                LcSeverity::Warning => "WARN".yellow().bold(),
                LcSeverity::Info => "INFO".blue(),
            };

            print!("  [{}] ", severity_str);
            print!("{}: ", issue.code.dimmed());
            println!("{}", issue.message);

            if !issue.location.is_empty() {
                println!("         at {}", issue.location.dimmed());
            }
        }
    }
}
