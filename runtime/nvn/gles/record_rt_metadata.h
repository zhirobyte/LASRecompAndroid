#ifndef GLES_RECORD_RT_METADATA_H
#define GLES_RECORD_RT_METADATA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "record_coordinator.h"
#include "texture_snapshot.h"

#define GLES_RECORD_RT_MAX_COLORS 8

/*
 * E167D3G_IMMUTABLE_RT_METADATA
 *
 * Metadata contains copied values only.
 * No live NvnTexture pointers or GL objects.
 *
 * The record identity must be validated separately
 * before this metadata is trusted at playback.
 */
typedef struct {
    GlesRecordCoordinatorKey record_key;

    uint32_t color_count;
    uint8_t color_present[GLES_RECORD_RT_MAX_COLORS];
    GlesTextureSnapshotKey colors[GLES_RECORD_RT_MAX_COLORS];

    uint8_t depth_present;
    GlesTextureSnapshotKey depth;

    uint8_t color_view_present[GLES_RECORD_RT_MAX_COLORS];
    uint8_t depth_view_present;

    bool valid;
} GlesRecordRtMetadata;

bool gles_record_rt_metadata_validate(
    const GlesRecordRtMetadata *metadata);

bool gles_record_rt_metadata_matches(
    const GlesRecordRtMetadata *metadata,
    GlesRecordCoordinatorKey record_key);

#endif
