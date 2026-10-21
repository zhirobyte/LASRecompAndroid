#include "record_rt_validation.h"

GlesRtValidationResult gles_record_rt_validate(
    GlesRecordCoordinator *records,
    GlesSnapshotRegistry *textures,
    const GlesRecordRtMetadata *metadata)
{
    if (!records || !textures || !metadata)
        return GLES_RT_VALIDATION_INVALID_ARGUMENT;

    if (!gles_record_rt_metadata_validate(metadata))
        return GLES_RT_VALIDATION_INVALID_METADATA;

    if (!gles_record_coordinator_lookup(
            records, metadata->record_key, NULL))
        return GLES_RT_VALIDATION_STALE_RECORD;

    for (uint32_t i = 0; i < metadata->color_count; ++i) {
        if (!metadata->color_present[i])
            continue;

        GlesTextureSnapshot snapshot;
        if (!gles_snapshot_registry_lookup(
                textures, metadata->colors[i], &snapshot))
            return GLES_RT_VALIDATION_STALE_COLOR;
    }

    GlesTextureSnapshot depth_snapshot;
    if (metadata->depth_present &&
        !gles_snapshot_registry_lookup(
            textures, metadata->depth, &depth_snapshot))
        return GLES_RT_VALIDATION_STALE_DEPTH;

    return GLES_RT_VALIDATION_OK;
}
