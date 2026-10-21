#include "snapshot_registry.h"

#include <limits.h>
#include <string.h>

bool gles_snapshot_registry_init(GlesSnapshotRegistry *registry)
{
    if (!registry)
        return false;

    memset(registry, 0, sizeof(*registry));

    if (pthread_mutex_init(&registry->mutex, NULL) != 0)
        return false;

    registry->initialized = true;
    return true;
}

void gles_snapshot_registry_destroy(GlesSnapshotRegistry *registry)
{
    if (!registry || !registry->initialized)
        return;

    /* No concurrent users are permitted during destruction. */
    pthread_mutex_destroy(&registry->mutex);
    registry->initialized = false;
}

bool gles_snapshot_registry_register(
    GlesSnapshotRegistry *registry,
    const GlesTextureSnapshot *metadata,
    GlesTextureSnapshotKey *key)
{
    if (!registry || !registry->initialized || !metadata || !key)
        return false;

    if (!metadata->key.pointer_value ||
        !metadata->key.debug_id ||
        !metadata->width ||
        !metadata->height ||
        !metadata->depth ||
        !metadata->levels)
        return false;

    if (pthread_mutex_lock(&registry->mutex) != 0)
        return false;

    GlesSnapshotRegistrySlot *free_slot = NULL;
    bool duplicate = false;

    for (size_t i = 0; i < GLES_SNAPSHOT_REGISTRY_CAPACITY; i++) {
        GlesSnapshotRegistrySlot *slot = &registry->slots[i];

        if (slot->occupied &&
            slot->snapshot.key.pointer_value ==
                metadata->key.pointer_value) {
            duplicate = true;
            break;
        }

        if (!slot->occupied && !free_slot)
            free_slot = slot;
    }

    bool ok = false;

    if (!duplicate && free_slot &&
        registry->next_generation != UINT64_MAX) {
        uint64_t generation = ++registry->next_generation;

        GlesTextureSnapshot snapshot = *metadata;
        snapshot.key.generation = generation;

        free_slot->snapshot = snapshot;
        free_slot->occupied = true;

        *key = snapshot.key;
        ok = true;
    }

    pthread_mutex_unlock(&registry->mutex);
    return ok;
}

bool gles_snapshot_registry_lookup(
    GlesSnapshotRegistry *registry,
    GlesTextureSnapshotKey key,
    GlesTextureSnapshot *out)
{
    if (!registry || !registry->initialized || !out)
        return false;

    if (pthread_mutex_lock(&registry->mutex) != 0)
        return false;

    bool found = false;

    for (size_t i = 0; i < GLES_SNAPSHOT_REGISTRY_CAPACITY; i++) {
        const GlesSnapshotRegistrySlot *slot = &registry->slots[i];

        if (slot->occupied &&
            gles_texture_snapshot_matches(&slot->snapshot, key)) {
            *out = slot->snapshot;
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&registry->mutex);
    return found;
}

bool gles_snapshot_registry_retire(
    GlesSnapshotRegistry *registry,
    GlesTextureSnapshotKey key)
{
    if (!registry || !registry->initialized)
        return false;

    if (pthread_mutex_lock(&registry->mutex) != 0)
        return false;

    bool retired = false;

    for (size_t i = 0; i < GLES_SNAPSHOT_REGISTRY_CAPACITY; i++) {
        GlesSnapshotRegistrySlot *slot = &registry->slots[i];

        if (slot->occupied &&
            gles_texture_snapshot_matches(&slot->snapshot, key)) {
            memset(&slot->snapshot, 0, sizeof(slot->snapshot));
            slot->occupied = false;
            retired = true;
            break;
        }
    }

    pthread_mutex_unlock(&registry->mutex);
    return retired;
}

size_t gles_snapshot_registry_count(GlesSnapshotRegistry *registry)
{
    if (!registry || !registry->initialized)
        return 0;

    if (pthread_mutex_lock(&registry->mutex) != 0)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < GLES_SNAPSHOT_REGISTRY_CAPACITY; i++)
        count += registry->slots[i].occupied ? 1u : 0u;

    pthread_mutex_unlock(&registry->mutex);
    return count;
}
