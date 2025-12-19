/**
 * Basic test for librecad_core
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "librecad_core.h"

int main(int argc, char* argv[]) {
    printf("librecad_core version: %s\n", lc_version());

    /* Test format detection */
    printf("\nFormat detection tests:\n");
    printf("  test.dxf -> %d (expected %d)\n", lc_detect_format("test.dxf"), LC_FORMAT_DXF);
    printf("  test.DXF -> %d (expected %d)\n", lc_detect_format("test.DXF"), LC_FORMAT_DXF);
    printf("  test.jww -> %d (expected %d)\n", lc_detect_format("test.jww"), LC_FORMAT_JWW);
    printf("  test.txt -> %d (expected %d)\n", lc_detect_format("test.txt"), LC_FORMAT_UNKNOWN);

    /* If a test file is provided, try to open it */
    if (argc > 1) {
        const char* filename = argv[1];
        printf("\nOpening file: %s\n", filename);

        LcDocument* doc = lc_document_open(filename);
        if (!doc) {
            printf("Error: %s\n", lc_last_error());
            return 1;
        }

        printf("File opened successfully!\n");

        /* Get info */
        LcFileInfo* info = lc_document_get_info(doc, LC_DETAIL_NORMAL);
        if (info) {
            printf("\nFile info:\n");
            printf("  Format: %d\n", info->format);
            printf("  DXF Version: %s\n", info->dxf_version ? info->dxf_version : "N/A");
            printf("  Layers: %d\n", info->layer_count);
            printf("  Blocks: %d\n", info->block_count);
            printf("  Entities: %d\n", info->entity_count);
            printf("  Bounds: (%.2f, %.2f) - (%.2f, %.2f)\n",
                   info->bounds.min.x, info->bounds.min.y,
                   info->bounds.max.x, info->bounds.max.y);

            /* Print JSON */
            char* json = lc_file_info_to_json(info);
            if (json) {
                printf("\nJSON output:\n%s\n", json);
                lc_string_free(json);
            }

            lc_file_info_free(info);
        }

        /* Validate */
        LcValidationResult* result = lc_document_validate(doc);
        if (result) {
            printf("\nValidation result:\n");
            printf("  Valid: %s\n", result->is_valid ? "yes" : "no");
            printf("  Issues: %d\n", result->issue_count);

            for (int i = 0; i < result->issue_count; i++) {
                printf("    [%s] %s: %s\n",
                       result->issues[i].severity == LC_SEVERITY_ERROR ? "ERROR" :
                       result->issues[i].severity == LC_SEVERITY_WARNING ? "WARN" : "INFO",
                       result->issues[i].code,
                       result->issues[i].message);
            }

            lc_validation_result_free(result);
        }

        lc_document_close(doc);
    }

    printf("\nAll tests passed!\n");
    return 0;
}
