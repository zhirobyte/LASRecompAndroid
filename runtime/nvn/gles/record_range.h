#ifndef GLES_RECORD_RANGE_H
#define GLES_RECORD_RANGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t start;
    size_t size;
} GlesRecordRange;

/*
 * E167D3E_RANGE_MODEL
 *
 * Ranges are half-open: [start, start + size).
 * Zero-length ranges and address overflow are invalid.
 *
 * This module performs arithmetic only.
 * It never dereferences command-memory addresses.
 */
bool gles_record_range_valid(GlesRecordRange range);

bool gles_record_range_overlaps(
    GlesRecordRange a,
    GlesRecordRange b);

bool gles_record_range_contains(
    GlesRecordRange outer,
    GlesRecordRange inner);

#endif
