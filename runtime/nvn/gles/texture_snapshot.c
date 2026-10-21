#include "texture_snapshot.h"

bool gles_texture_snapshot_valid(
    const GlesTextureSnapshot *snapshot)
{
    return snapshot != 0 &&
           snapshot->key.pointer_value != 0 &&
           snapshot->key.generation != 0 &&
           snapshot->key.debug_id != 0 &&
           snapshot->width != 0 &&
           snapshot->height != 0 &&
           snapshot->depth != 0 &&
           snapshot->levels != 0;
}

bool gles_texture_snapshot_matches(
    const GlesTextureSnapshot *snapshot,
    GlesTextureSnapshotKey key)
{
    return gles_texture_snapshot_valid(snapshot) &&
           snapshot->key.pointer_value == key.pointer_value &&
           snapshot->key.generation == key.generation &&
           snapshot->key.debug_id == key.debug_id;
}

bool gles_texture_snapshot_copy(
    GlesTextureSnapshot *destination,
    const GlesTextureSnapshot *source)
{
    if (!destination || !gles_texture_snapshot_valid(source))
        return false;

    *destination = *source;
    return true;
}
