#include "record_rt_metadata.h"

static bool texture_key_valid(GlesTextureSnapshotKey key)
{
    return key.pointer_value != 0 &&
           key.generation != 0;
}

bool gles_record_rt_metadata_validate(
    const GlesRecordRtMetadata *metadata)
{
    if (!metadata || !metadata->valid)
        return false;

    if (!metadata->record_key.address ||
        !metadata->record_key.generation)
        return false;

    if (metadata->color_count > GLES_RECORD_RT_MAX_COLORS)
        return false;

    for (uint32_t i = 0; i < GLES_RECORD_RT_MAX_COLORS; ++i) {
        if (metadata->color_present[i] > 1 ||
            metadata->color_view_present[i] > 1)
            return false;

        if (i >= metadata->color_count &&
            metadata->color_present[i])
            return false;

        if (metadata->color_present[i] &&
            !texture_key_valid(metadata->colors[i]))
            return false;

        if (metadata->color_view_present[i] &&
            !metadata->color_present[i])
            return false;
    }

    if (metadata->depth_present > 1 ||
        metadata->depth_view_present > 1)
        return false;

    if (metadata->depth_present &&
        !texture_key_valid(metadata->depth))
        return false;

    if (metadata->depth_view_present &&
        !metadata->depth_present)
        return false;

    return true;
}

bool gles_record_rt_metadata_matches(
    const GlesRecordRtMetadata *metadata,
    GlesRecordCoordinatorKey record_key)
{
    if (!gles_record_rt_metadata_validate(metadata))
        return false;

    return metadata->record_key.address == record_key.address &&
           metadata->record_key.generation == record_key.generation;
}
