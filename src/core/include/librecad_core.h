/**
 * cadutil_core - C API for cadutil (CAD Utility CLI)
 *
 * Provides conversion, info extraction, and validation for DXF/JWW files.
 * This is an unofficial, independent library not affiliated with any CAD software project.
 */

#ifndef LIBRECAD_CORE_H
#define LIBRECAD_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version info */
#define LIBRECAD_CORE_VERSION_MAJOR 0
#define LIBRECAD_CORE_VERSION_MINOR 1
#define LIBRECAD_CORE_VERSION_PATCH 0

/* Error codes */
typedef enum {
    LC_OK = 0,
    LC_ERR_FILE_NOT_FOUND = 1,
    LC_ERR_INVALID_FORMAT = 2,
    LC_ERR_READ_ERROR = 3,
    LC_ERR_WRITE_ERROR = 4,
    LC_ERR_UNSUPPORTED_VERSION = 5,
    LC_ERR_OUT_OF_MEMORY = 6,
    LC_ERR_INVALID_ARGUMENT = 7,
    LC_ERR_UNKNOWN = 99
} LcError;

/* File format types */
typedef enum {
    LC_FORMAT_UNKNOWN = 0,
    LC_FORMAT_DXF = 1,
    LC_FORMAT_DWG = 2,
    LC_FORMAT_JWW = 3,
    LC_FORMAT_JWC = 4
} LcFormat;

/* DXF version for export */
typedef enum {
    LC_DXF_VERSION_R12 = 12,
    LC_DXF_VERSION_R14 = 14,
    LC_DXF_VERSION_2000 = 2000,
    LC_DXF_VERSION_2004 = 2004,
    LC_DXF_VERSION_2007 = 2007,
    LC_DXF_VERSION_2010 = 2010,
    LC_DXF_VERSION_2013 = 2013,
    LC_DXF_VERSION_2018 = 2018
} LcDxfVersion;

/* Entity types */
typedef enum {
    LC_ENTITY_UNKNOWN = 0,
    LC_ENTITY_POINT = 1,
    LC_ENTITY_LINE = 2,
    LC_ENTITY_CIRCLE = 3,
    LC_ENTITY_ARC = 4,
    LC_ENTITY_ELLIPSE = 5,
    LC_ENTITY_POLYLINE = 6,
    LC_ENTITY_LWPOLYLINE = 7,
    LC_ENTITY_SPLINE = 8,
    LC_ENTITY_TEXT = 9,
    LC_ENTITY_MTEXT = 10,
    LC_ENTITY_INSERT = 11,
    LC_ENTITY_HATCH = 12,
    LC_ENTITY_DIMENSION = 13,
    LC_ENTITY_LEADER = 14,
    LC_ENTITY_SOLID = 15,
    LC_ENTITY_TRACE = 16,
    LC_ENTITY_3DFACE = 17,
    LC_ENTITY_IMAGE = 18,
    LC_ENTITY_VIEWPORT = 19
} LcEntityType;

/* Validation severity levels */
typedef enum {
    LC_SEVERITY_INFO = 0,
    LC_SEVERITY_WARNING = 1,
    LC_SEVERITY_ERROR = 2
} LcSeverity;

/* Detail levels for info output */
typedef enum {
    LC_DETAIL_SUMMARY = 0,     /* File overview only */
    LC_DETAIL_NORMAL = 1,      /* Layers, blocks, entity counts */
    LC_DETAIL_VERBOSE = 2,     /* All entities with basic properties */
    LC_DETAIL_FULL = 3         /* Full entity details including geometry */
} LcDetailLevel;

/* ============================================================================
 * Document handle (opaque pointer)
 * ============================================================================ */
typedef struct LcDocument LcDocument;

/* ============================================================================
 * Basic info structures
 * ============================================================================ */

typedef struct {
    double x;
    double y;
    double z;
} LcPoint3D;

typedef struct {
    LcPoint3D min;
    LcPoint3D max;
} LcBoundingBox;

typedef struct {
    char* name;
    int color;
    char* line_type;
    double line_weight;
    int is_off;
    int is_frozen;
    int is_locked;
} LcLayerInfo;

typedef struct {
    char* name;
    LcPoint3D base_point;
    int entity_count;
} LcBlockInfo;

typedef struct {
    LcEntityType type;
    char* layer;
    int color;
    char* line_type;
    double line_weight;
    int handle;
    /* Geometry data (union-like, depends on type) */
    union {
        struct { LcPoint3D point; } point;
        struct { LcPoint3D start; LcPoint3D end; } line;
        struct { LcPoint3D center; double radius; } circle;
        struct { LcPoint3D center; double radius; double start_angle; double end_angle; } arc;
        struct { LcPoint3D center; double major_radius; double minor_radius; double rotation; } ellipse;
        struct { char* text; LcPoint3D position; double height; double rotation; } text;
        struct { char* block_name; LcPoint3D position; double scale_x; double scale_y; double rotation; } insert;
        struct { int vertex_count; int is_closed; } polyline;
        struct { int control_point_count; int degree; int is_closed; } spline;
    } data;
} LcEntityInfo;

typedef struct {
    LcSeverity severity;
    char* code;
    char* message;
    char* location;  /* e.g., "entity #123" or "layer 'foo'" */
} LcValidationIssue;

typedef struct {
    /* File info */
    char* filename;
    LcFormat format;
    char* dxf_version;

    /* Statistics */
    int layer_count;
    int block_count;
    int entity_count;
    LcBoundingBox bounds;

    /* Detailed info (populated based on detail level) */
    LcLayerInfo* layers;
    int layers_len;

    LcBlockInfo* blocks;
    int blocks_len;

    LcEntityInfo* entities;
    int entities_len;

    /* Entity type counts */
    int entity_counts[20];  /* Indexed by LcEntityType */
} LcFileInfo;

typedef struct {
    int is_valid;
    int issue_count;
    LcValidationIssue* issues;
} LcValidationResult;

/* ============================================================================
 * Core API Functions
 * ============================================================================ */

/**
 * Get library version string
 */
const char* lc_version(void);

/**
 * Get last error message (thread-local)
 */
const char* lc_last_error(void);

/**
 * Detect file format from filename/extension
 */
LcFormat lc_detect_format(const char* filename);

/* ============================================================================
 * Document Operations
 * ============================================================================ */

/**
 * Open a document (DXF or JWW)
 * Returns NULL on error, check lc_last_error()
 */
LcDocument* lc_document_open(const char* filename);

/**
 * Save document to file
 * For DXF output, use version parameter
 */
LcError lc_document_save(LcDocument* doc, const char* filename, LcDxfVersion version);

/**
 * Close and free document
 */
void lc_document_close(LcDocument* doc);

/* ============================================================================
 * Conversion API
 * ============================================================================ */

/**
 * Convert file from one format to another
 * This is a convenience function that opens, converts, and saves
 */
LcError lc_convert(const char* input_file, const char* output_file, LcDxfVersion dxf_version);

/* ============================================================================
 * Info API
 * ============================================================================ */

/**
 * Get file information at specified detail level
 * Caller must free with lc_file_info_free()
 */
LcFileInfo* lc_get_file_info(const char* filename, LcDetailLevel detail);

/**
 * Get file info from open document
 */
LcFileInfo* lc_document_get_info(LcDocument* doc, LcDetailLevel detail);

/**
 * Free file info structure
 */
void lc_file_info_free(LcFileInfo* info);

/**
 * Export file info as JSON string
 * Caller must free returned string with lc_string_free()
 */
char* lc_file_info_to_json(const LcFileInfo* info);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * Validate a file
 * Caller must free with lc_validation_result_free()
 */
LcValidationResult* lc_validate(const char* filename);

/**
 * Validate an open document
 */
LcValidationResult* lc_document_validate(LcDocument* doc);

/**
 * Free validation result
 */
void lc_validation_result_free(LcValidationResult* result);

/**
 * Export validation result as JSON string
 * Caller must free returned string with lc_string_free()
 */
char* lc_validation_result_to_json(const LcValidationResult* result);

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/**
 * Free a string allocated by this library
 */
void lc_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif /* LIBRECAD_CORE_H */
