//! FFI bindings to librecad_core

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_double, c_int};

/// Error codes from the C library
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum LcError {
    Ok = 0,
    FileNotFound = 1,
    InvalidFormat = 2,
    ReadError = 3,
    WriteError = 4,
    UnsupportedVersion = 5,
    OutOfMemory = 6,
    InvalidArgument = 7,
    Unknown = 99,
}

/// File format types
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum LcFormat {
    Unknown = 0,
    Dxf = 1,
    Dwg = 2,
    Jww = 3,
    Jwc = 4,
}

impl LcFormat {
    pub fn as_str(&self) -> &'static str {
        match self {
            LcFormat::Unknown => "Unknown",
            LcFormat::Dxf => "DXF",
            LcFormat::Dwg => "DWG",
            LcFormat::Jww => "JWW",
            LcFormat::Jwc => "JWC",
        }
    }
}

/// DXF version for export
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LcDxfVersion {
    R12 = 12,
    R14 = 14,
    V2000 = 2000,
    V2004 = 2004,
    V2007 = 2007,
    V2010 = 2010,
    V2013 = 2013,
    V2018 = 2018,
}

impl std::str::FromStr for LcDxfVersion {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "r12" | "12" => Ok(LcDxfVersion::R12),
            "r14" | "14" => Ok(LcDxfVersion::R14),
            "2000" => Ok(LcDxfVersion::V2000),
            "2004" => Ok(LcDxfVersion::V2004),
            "2007" => Ok(LcDxfVersion::V2007),
            "2010" => Ok(LcDxfVersion::V2010),
            "2013" => Ok(LcDxfVersion::V2013),
            "2018" => Ok(LcDxfVersion::V2018),
            _ => Err(format!("Unknown DXF version: {}", s)),
        }
    }
}

/// Entity types
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum LcEntityType {
    Unknown = 0,
    Point = 1,
    Line = 2,
    Circle = 3,
    Arc = 4,
    Ellipse = 5,
    Polyline = 6,
    LwPolyline = 7,
    Spline = 8,
    Text = 9,
    MText = 10,
    Insert = 11,
    Hatch = 12,
    Dimension = 13,
    Leader = 14,
    Solid = 15,
    Trace = 16,
    Face3D = 17,
    Image = 18,
    Viewport = 19,
}

impl LcEntityType {
    pub fn as_str(&self) -> &'static str {
        match self {
            LcEntityType::Unknown => "UNKNOWN",
            LcEntityType::Point => "POINT",
            LcEntityType::Line => "LINE",
            LcEntityType::Circle => "CIRCLE",
            LcEntityType::Arc => "ARC",
            LcEntityType::Ellipse => "ELLIPSE",
            LcEntityType::Polyline => "POLYLINE",
            LcEntityType::LwPolyline => "LWPOLYLINE",
            LcEntityType::Spline => "SPLINE",
            LcEntityType::Text => "TEXT",
            LcEntityType::MText => "MTEXT",
            LcEntityType::Insert => "INSERT",
            LcEntityType::Hatch => "HATCH",
            LcEntityType::Dimension => "DIMENSION",
            LcEntityType::Leader => "LEADER",
            LcEntityType::Solid => "SOLID",
            LcEntityType::Trace => "TRACE",
            LcEntityType::Face3D => "3DFACE",
            LcEntityType::Image => "IMAGE",
            LcEntityType::Viewport => "VIEWPORT",
        }
    }
}

/// Validation severity levels
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum LcSeverity {
    Info = 0,
    Warning = 1,
    Error = 2,
}

impl LcSeverity {
    #[allow(dead_code)]
    pub fn as_str(&self) -> &'static str {
        match self {
            LcSeverity::Info => "info",
            LcSeverity::Warning => "warning",
            LcSeverity::Error => "error",
        }
    }
}

/// Detail levels for info output
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LcDetailLevel {
    Summary = 0,
    Normal = 1,
    Verbose = 2,
    Full = 3,
}

impl std::str::FromStr for LcDetailLevel {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "summary" | "s" | "0" => Ok(LcDetailLevel::Summary),
            "normal" | "n" | "1" => Ok(LcDetailLevel::Normal),
            "verbose" | "v" | "2" => Ok(LcDetailLevel::Verbose),
            "full" | "f" | "3" => Ok(LcDetailLevel::Full),
            _ => Err(format!("Unknown detail level: {}", s)),
        }
    }
}

/// 3D point
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct LcPoint3D {
    pub x: c_double,
    pub y: c_double,
    pub z: c_double,
}

/// Bounding box
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct LcBoundingBox {
    pub min: LcPoint3D,
    pub max: LcPoint3D,
}

/// Layer info
#[repr(C)]
pub struct LcLayerInfo {
    pub name: *mut c_char,
    pub color: c_int,
    pub line_type: *mut c_char,
    pub line_weight: c_double,
    pub is_off: c_int,
    pub is_frozen: c_int,
    pub is_locked: c_int,
}

/// Block info
#[repr(C)]
pub struct LcBlockInfo {
    pub name: *mut c_char,
    pub base_point: LcPoint3D,
    pub entity_count: c_int,
}

/// Entity info (simplified)
/// Note: data union is 56 bytes, total struct is 104 bytes on 64-bit
#[repr(C)]
pub struct LcEntityInfo {
    pub entity_type: LcEntityType,
    pub layer: *mut c_char,
    pub color: c_int,
    pub line_type: *mut c_char,
    pub line_weight: c_double,
    pub handle: c_int,
    pub data: [u8; 56], // Union data placeholder (matches C struct)
}

/// Validation issue
#[repr(C)]
pub struct LcValidationIssue {
    pub severity: LcSeverity,
    pub code: *mut c_char,
    pub message: *mut c_char,
    pub location: *mut c_char,
}

/// File info structure
#[repr(C)]
pub struct LcFileInfo {
    pub filename: *mut c_char,
    pub format: LcFormat,
    pub dxf_version: *mut c_char,
    pub layer_count: c_int,
    pub block_count: c_int,
    pub entity_count: c_int,
    pub bounds: LcBoundingBox,
    pub layers: *mut LcLayerInfo,
    pub layers_len: c_int,
    pub blocks: *mut LcBlockInfo,
    pub blocks_len: c_int,
    pub entities: *mut LcEntityInfo,
    pub entities_len: c_int,
    pub entity_counts: [c_int; 20],
}

/// Validation result
#[repr(C)]
pub struct LcValidationResult {
    pub is_valid: c_int,
    pub issue_count: c_int,
    pub issues: *mut LcValidationIssue,
}

/// Document handle (opaque)
#[repr(C)]
#[allow(dead_code)]
pub struct LcDocument {
    _private: [u8; 0],
}

// External C functions
#[allow(dead_code)]
extern "C" {
    pub fn lc_version() -> *const c_char;
    pub fn lc_last_error() -> *const c_char;
    pub fn lc_detect_format(filename: *const c_char) -> LcFormat;

    pub fn lc_document_open(filename: *const c_char) -> *mut LcDocument;
    pub fn lc_document_save(
        doc: *mut LcDocument,
        filename: *const c_char,
        version: LcDxfVersion,
    ) -> LcError;
    pub fn lc_document_close(doc: *mut LcDocument);

    pub fn lc_convert(
        input_file: *const c_char,
        output_file: *const c_char,
        dxf_version: LcDxfVersion,
    ) -> LcError;

    pub fn lc_get_file_info(filename: *const c_char, detail: LcDetailLevel) -> *mut LcFileInfo;
    pub fn lc_document_get_info(doc: *mut LcDocument, detail: LcDetailLevel) -> *mut LcFileInfo;
    pub fn lc_file_info_free(info: *mut LcFileInfo);
    pub fn lc_file_info_to_json(info: *const LcFileInfo) -> *mut c_char;

    pub fn lc_validate(filename: *const c_char) -> *mut LcValidationResult;
    pub fn lc_document_validate(doc: *mut LcDocument) -> *mut LcValidationResult;
    pub fn lc_validation_result_free(result: *mut LcValidationResult);
    pub fn lc_validation_result_to_json(result: *const LcValidationResult) -> *mut c_char;

    pub fn lc_string_free(s: *mut c_char);
}

// Safe Rust wrappers

/// Get library version
pub fn version() -> String {
    unsafe {
        let ptr = lc_version();
        if ptr.is_null() {
            return String::new();
        }
        CStr::from_ptr(ptr).to_string_lossy().into_owned()
    }
}

/// Get last error message
pub fn last_error() -> String {
    unsafe {
        let ptr = lc_last_error();
        if ptr.is_null() {
            return String::new();
        }
        CStr::from_ptr(ptr).to_string_lossy().into_owned()
    }
}

/// Detect file format from filename
pub fn detect_format(filename: &str) -> LcFormat {
    let c_filename = CString::new(filename).unwrap();
    unsafe { lc_detect_format(c_filename.as_ptr()) }
}

/// Convert a file
pub fn convert(input: &str, output: &str, dxf_version: LcDxfVersion) -> Result<(), String> {
    let c_input = CString::new(input).unwrap();
    let c_output = CString::new(output).unwrap();

    let result = unsafe { lc_convert(c_input.as_ptr(), c_output.as_ptr(), dxf_version) };

    if result == LcError::Ok {
        Ok(())
    } else {
        Err(last_error())
    }
}

/// Get file info as JSON string
pub fn get_file_info_json(filename: &str, detail: LcDetailLevel) -> Result<String, String> {
    let c_filename = CString::new(filename).unwrap();

    unsafe {
        let info = lc_get_file_info(c_filename.as_ptr(), detail);
        if info.is_null() {
            return Err(last_error());
        }

        let json_ptr = lc_file_info_to_json(info);
        lc_file_info_free(info);

        if json_ptr.is_null() {
            return Err("Failed to convert to JSON".to_string());
        }

        let json = CStr::from_ptr(json_ptr).to_string_lossy().into_owned();
        lc_string_free(json_ptr);

        Ok(json)
    }
}

/// Get file info structure
pub fn get_file_info(filename: &str, detail: LcDetailLevel) -> Result<FileInfo, String> {
    let c_filename = CString::new(filename).unwrap();

    unsafe {
        let info = lc_get_file_info(c_filename.as_ptr(), detail);
        if info.is_null() {
            return Err(last_error());
        }

        let result = FileInfo::from_raw(info);
        lc_file_info_free(info);

        Ok(result)
    }
}

/// Validate a file and return JSON result
pub fn validate_json(filename: &str) -> Result<String, String> {
    let c_filename = CString::new(filename).unwrap();

    unsafe {
        let result = lc_validate(c_filename.as_ptr());
        if result.is_null() {
            return Err(last_error());
        }

        let json_ptr = lc_validation_result_to_json(result);
        lc_validation_result_free(result);

        if json_ptr.is_null() {
            return Err("Failed to convert to JSON".to_string());
        }

        let json = CStr::from_ptr(json_ptr).to_string_lossy().into_owned();
        lc_string_free(json_ptr);

        Ok(json)
    }
}

/// Validate a file
pub fn validate(filename: &str) -> Result<ValidationResult, String> {
    let c_filename = CString::new(filename).unwrap();

    unsafe {
        let result = lc_validate(c_filename.as_ptr());
        if result.is_null() {
            return Err(last_error());
        }

        let res = ValidationResult::from_raw(result);
        lc_validation_result_free(result);

        Ok(res)
    }
}

// High-level Rust types

/// Layer information (Rust-owned)
#[derive(Debug, Clone)]
pub struct LayerInfo {
    pub name: String,
    pub color: i32,
    pub line_type: String,
    #[allow(dead_code)]
    pub line_weight: f64,
    pub is_off: bool,
    pub is_frozen: bool,
    pub is_locked: bool,
}

/// Block information (Rust-owned)
#[derive(Debug, Clone)]
pub struct BlockInfo {
    pub name: String,
    pub base_point: (f64, f64, f64),
    pub entity_count: i32,
}

/// Entity information (Rust-owned)
#[derive(Debug, Clone)]
pub struct EntityInfo {
    pub entity_type: LcEntityType,
    pub layer: String,
    pub color: i32,
    #[allow(dead_code)]
    pub line_type: String,
    #[allow(dead_code)]
    pub line_weight: f64,
    #[allow(dead_code)]
    pub handle: i32,
}

/// File information (Rust-owned)
#[derive(Debug, Clone)]
pub struct FileInfo {
    pub filename: String,
    pub format: LcFormat,
    pub dxf_version: String,
    pub layer_count: i32,
    pub block_count: i32,
    pub entity_count: i32,
    pub bounds: ((f64, f64, f64), (f64, f64, f64)),
    pub layers: Vec<LayerInfo>,
    pub blocks: Vec<BlockInfo>,
    pub entities: Vec<EntityInfo>,
    pub entity_counts: [i32; 20],
}

impl FileInfo {
    unsafe fn from_raw(raw: *const LcFileInfo) -> Self {
        let info = &*raw;

        let filename = if info.filename.is_null() {
            String::new()
        } else {
            CStr::from_ptr(info.filename).to_string_lossy().into_owned()
        };

        let dxf_version = if info.dxf_version.is_null() {
            String::new()
        } else {
            CStr::from_ptr(info.dxf_version)
                .to_string_lossy()
                .into_owned()
        };

        let mut layers = Vec::new();
        if !info.layers.is_null() {
            for i in 0..info.layers_len {
                let layer = &*info.layers.offset(i as isize);
                layers.push(LayerInfo {
                    name: if layer.name.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(layer.name).to_string_lossy().into_owned()
                    },
                    color: layer.color,
                    line_type: if layer.line_type.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(layer.line_type)
                            .to_string_lossy()
                            .into_owned()
                    },
                    line_weight: layer.line_weight,
                    is_off: layer.is_off != 0,
                    is_frozen: layer.is_frozen != 0,
                    is_locked: layer.is_locked != 0,
                });
            }
        }

        let mut blocks = Vec::new();
        if !info.blocks.is_null() {
            for i in 0..info.blocks_len {
                let block = &*info.blocks.offset(i as isize);
                blocks.push(BlockInfo {
                    name: if block.name.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(block.name).to_string_lossy().into_owned()
                    },
                    base_point: (
                        block.base_point.x,
                        block.base_point.y,
                        block.base_point.z,
                    ),
                    entity_count: block.entity_count,
                });
            }
        }

        let mut entities = Vec::new();
        if !info.entities.is_null() {
            for i in 0..info.entities_len {
                let entity = &*info.entities.offset(i as isize);
                entities.push(EntityInfo {
                    entity_type: entity.entity_type,
                    layer: if entity.layer.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(entity.layer).to_string_lossy().into_owned()
                    },
                    color: entity.color,
                    line_type: if entity.line_type.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(entity.line_type)
                            .to_string_lossy()
                            .into_owned()
                    },
                    line_weight: entity.line_weight,
                    handle: entity.handle,
                });
            }
        }

        FileInfo {
            filename,
            format: info.format,
            dxf_version,
            layer_count: info.layer_count,
            block_count: info.block_count,
            entity_count: info.entity_count,
            bounds: (
                (info.bounds.min.x, info.bounds.min.y, info.bounds.min.z),
                (info.bounds.max.x, info.bounds.max.y, info.bounds.max.z),
            ),
            layers,
            blocks,
            entities,
            entity_counts: info.entity_counts,
        }
    }
}

/// Validation issue (Rust-owned)
#[derive(Debug, Clone)]
pub struct Issue {
    pub severity: LcSeverity,
    pub code: String,
    pub message: String,
    pub location: String,
}

/// Validation result (Rust-owned)
#[derive(Debug, Clone)]
pub struct ValidationResult {
    pub is_valid: bool,
    pub issues: Vec<Issue>,
}

impl ValidationResult {
    unsafe fn from_raw(raw: *const LcValidationResult) -> Self {
        let result = &*raw;

        let mut issues = Vec::new();
        if !result.issues.is_null() {
            for i in 0..result.issue_count {
                let issue = &*result.issues.offset(i as isize);
                issues.push(Issue {
                    severity: issue.severity,
                    code: if issue.code.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(issue.code).to_string_lossy().into_owned()
                    },
                    message: if issue.message.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(issue.message).to_string_lossy().into_owned()
                    },
                    location: if issue.location.is_null() {
                        String::new()
                    } else {
                        CStr::from_ptr(issue.location).to_string_lossy().into_owned()
                    },
                });
            }
        }

        ValidationResult {
            is_valid: result.is_valid != 0,
            issues,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dxf_version_parsing() {
        assert_eq!("r12".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::R12);
        assert_eq!("R12".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::R12);
        assert_eq!("12".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::R12);
        assert_eq!("r14".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::R14);
        assert_eq!("2000".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::V2000);
        assert_eq!("2004".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::V2004);
        assert_eq!("2007".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::V2007);
        assert_eq!("2010".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::V2010);
        assert_eq!("2013".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::V2013);
        assert_eq!("2018".parse::<LcDxfVersion>().unwrap(), LcDxfVersion::V2018);
    }

    #[test]
    fn test_dxf_version_parsing_invalid() {
        assert!("invalid".parse::<LcDxfVersion>().is_err());
        assert!("2025".parse::<LcDxfVersion>().is_err());
        assert!("".parse::<LcDxfVersion>().is_err());
    }

    #[test]
    fn test_detail_level_parsing() {
        assert_eq!("summary".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Summary);
        assert_eq!("s".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Summary);
        assert_eq!("0".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Summary);
        assert_eq!("normal".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Normal);
        assert_eq!("n".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Normal);
        assert_eq!("1".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Normal);
        assert_eq!("verbose".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Verbose);
        assert_eq!("v".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Verbose);
        assert_eq!("2".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Verbose);
        assert_eq!("full".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Full);
        assert_eq!("f".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Full);
        assert_eq!("3".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Full);
    }

    #[test]
    fn test_detail_level_parsing_case_insensitive() {
        assert_eq!("SUMMARY".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Summary);
        assert_eq!("Normal".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Normal);
        assert_eq!("VERBOSE".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Verbose);
        assert_eq!("Full".parse::<LcDetailLevel>().unwrap(), LcDetailLevel::Full);
    }

    #[test]
    fn test_detail_level_parsing_invalid() {
        assert!("invalid".parse::<LcDetailLevel>().is_err());
        assert!("4".parse::<LcDetailLevel>().is_err());
        assert!("".parse::<LcDetailLevel>().is_err());
    }

    #[test]
    fn test_format_as_str() {
        assert_eq!(LcFormat::Unknown.as_str(), "Unknown");
        assert_eq!(LcFormat::Dxf.as_str(), "DXF");
        assert_eq!(LcFormat::Dwg.as_str(), "DWG");
        assert_eq!(LcFormat::Jww.as_str(), "JWW");
        assert_eq!(LcFormat::Jwc.as_str(), "JWC");
    }

    #[test]
    fn test_entity_type_as_str() {
        assert_eq!(LcEntityType::Unknown.as_str(), "UNKNOWN");
        assert_eq!(LcEntityType::Point.as_str(), "POINT");
        assert_eq!(LcEntityType::Line.as_str(), "LINE");
        assert_eq!(LcEntityType::Circle.as_str(), "CIRCLE");
        assert_eq!(LcEntityType::Arc.as_str(), "ARC");
        assert_eq!(LcEntityType::Ellipse.as_str(), "ELLIPSE");
        assert_eq!(LcEntityType::Polyline.as_str(), "POLYLINE");
        assert_eq!(LcEntityType::LwPolyline.as_str(), "LWPOLYLINE");
        assert_eq!(LcEntityType::Spline.as_str(), "SPLINE");
        assert_eq!(LcEntityType::Text.as_str(), "TEXT");
        assert_eq!(LcEntityType::MText.as_str(), "MTEXT");
        assert_eq!(LcEntityType::Insert.as_str(), "INSERT");
        assert_eq!(LcEntityType::Hatch.as_str(), "HATCH");
        assert_eq!(LcEntityType::Dimension.as_str(), "DIMENSION");
        assert_eq!(LcEntityType::Leader.as_str(), "LEADER");
        assert_eq!(LcEntityType::Solid.as_str(), "SOLID");
        assert_eq!(LcEntityType::Trace.as_str(), "TRACE");
        assert_eq!(LcEntityType::Face3D.as_str(), "3DFACE");
        assert_eq!(LcEntityType::Image.as_str(), "IMAGE");
        assert_eq!(LcEntityType::Viewport.as_str(), "VIEWPORT");
    }

    #[test]
    fn test_severity_as_str() {
        assert_eq!(LcSeverity::Info.as_str(), "info");
        assert_eq!(LcSeverity::Warning.as_str(), "warning");
        assert_eq!(LcSeverity::Error.as_str(), "error");
    }

    #[test]
    fn test_error_enum_values() {
        assert_eq!(LcError::Ok as i32, 0);
        assert_eq!(LcError::FileNotFound as i32, 1);
        assert_eq!(LcError::InvalidFormat as i32, 2);
        assert_eq!(LcError::ReadError as i32, 3);
        assert_eq!(LcError::WriteError as i32, 4);
        assert_eq!(LcError::UnsupportedVersion as i32, 5);
        assert_eq!(LcError::OutOfMemory as i32, 6);
        assert_eq!(LcError::InvalidArgument as i32, 7);
        assert_eq!(LcError::Unknown as i32, 99);
    }

    #[test]
    fn test_format_enum_values() {
        assert_eq!(LcFormat::Unknown as i32, 0);
        assert_eq!(LcFormat::Dxf as i32, 1);
        assert_eq!(LcFormat::Dwg as i32, 2);
        assert_eq!(LcFormat::Jww as i32, 3);
        assert_eq!(LcFormat::Jwc as i32, 4);
    }

    #[test]
    fn test_detail_level_enum_values() {
        assert_eq!(LcDetailLevel::Summary as i32, 0);
        assert_eq!(LcDetailLevel::Normal as i32, 1);
        assert_eq!(LcDetailLevel::Verbose as i32, 2);
        assert_eq!(LcDetailLevel::Full as i32, 3);
    }

    #[test]
    fn test_severity_enum_values() {
        assert_eq!(LcSeverity::Info as i32, 0);
        assert_eq!(LcSeverity::Warning as i32, 1);
        assert_eq!(LcSeverity::Error as i32, 2);
    }
}
