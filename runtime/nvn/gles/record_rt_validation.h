#ifndef GLES_RECORD_RT_VALIDATION_H
#define GLES_RECORD_RT_VALIDATION_H

#include "record_coordinator.h"
#include "record_rt_metadata.h"
#include "snapshot_registry.h"

typedef enum {
    GLES_RT_VALIDATION_OK = 0,
    GLES_RT_VALIDATION_INVALID_ARGUMENT,
    GLES_RT_VALIDATION_INVALID_METADATA,
    GLES_RT_VALIDATION_STALE_RECORD,
    GLES_RT_VALIDATION_STALE_COLOR,
    GLES_RT_VALIDATION_STALE_DEPTH
} GlesRtValidationResult;

/*
 * E167D3G3:
 * Checks exact record identity and all referenced
 * texture snapshot generations.
 *
 * This is a point-in-time validation only.
 * Callers must separately guarantee object lifetime
 * during subsequent use.
 */
GlesRtValidationResult gles_record_rt_validate(
    GlesRecordCoordinator *records,
    GlesSnapshotRegistry *textures,
    const GlesRecordRtMetadata *metadata);

#endif
