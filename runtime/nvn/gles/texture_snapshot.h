#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * E156: Value-only metadata snapshot.
 * Never dereference identity.pointer during lookup.
 */
typedef struct {
    uintptr_t pointer_value;
    uint64_t generation;
    uint64_t debug_id;
} GlesTextureSnapshotKey;

typedef struct {
    GlesTextureSnapshotKey key;
    uint32_t target;
    uint32_t format;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t levels;
    uint32_t samples;
    uint32_t ds_mode;
    uint64_t stride;
    uint64_t pool_offset;
} GlesTextureSnapshot;

bool gles_texture_snapshot_valid(
    const GlesTextureSnapshot *snapshot);

bool gles_texture_snapshot_matches(
    const GlesTextureSnapshot *snapshot,
    GlesTextureSnapshotKey key);

bool gles_texture_snapshot_copy(
    GlesTextureSnapshot *destination,
    const GlesTextureSnapshot *source);
