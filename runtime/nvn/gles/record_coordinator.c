#include "record_coordinator.h"

#include <limits.h>
#include <string.h>

bool gles_record_coordinator_init(GlesRecordCoordinator *registry)
{
    if (!registry)
        return false;

    memset(registry, 0, sizeof(*registry));

    if (pthread_mutex_init(&registry->mutex, NULL) != 0)
        return false;

    registry->next_generation = 1;
    registry->initialized = true;
    return true;
}

void gles_record_coordinator_destroy(GlesRecordCoordinator *registry)
{
    if (!registry || !registry->initialized)
        return;

    pthread_mutex_destroy(&registry->mutex);
    registry->initialized = false;
}

static size_t invalidate_locked(
    GlesRecordCoordinator *registry,
    GlesRecordRange range)
{
    size_t retired = 0;

    for (size_t i = 0; i < GLES_RECORD_COORDINATOR_CAPACITY; ++i) {
        GlesRecordCoordinatorSlot *slot = &registry->slots[i];

        if (slot->occupied &&
            gles_record_range_overlaps(slot->range, range)) {
            slot->occupied = false;
            ++retired;
        }
    }

    return retired;
}

bool gles_record_coordinator_register(
    GlesRecordCoordinator *registry,
    GlesRecordRange range,
    GlesRecordCoordinatorKey *out)
{
    if (!registry || !registry->initialized || !out ||
        !gles_record_range_valid(range))
        return false;

    pthread_mutex_lock(&registry->mutex);

    if (!registry->next_generation ||
        registry->next_generation == UINT64_MAX) {
        pthread_mutex_unlock(&registry->mutex);
        return false;
    }

    invalidate_locked(registry, range);

    GlesRecordCoordinatorSlot *free_slot = NULL;

    for (size_t i = 0; i < GLES_RECORD_COORDINATOR_CAPACITY; ++i) {
        if (!registry->slots[i].occupied) {
            free_slot = &registry->slots[i];
            break;
        }
    }

    if (!free_slot) {
        pthread_mutex_unlock(&registry->mutex);
        return false;
    }

    GlesRecordCoordinatorKey key = {
        .address = range.start,
        .generation = registry->next_generation++
    };

    free_slot->range = range;
    free_slot->key = key;
    free_slot->occupied = true;

    *out = key;

    pthread_mutex_unlock(&registry->mutex);
    return true;
}

size_t gles_record_coordinator_invalidate(
    GlesRecordCoordinator *registry,
    GlesRecordRange range)
{
    if (!registry || !registry->initialized ||
        !gles_record_range_valid(range))
        return 0;

    pthread_mutex_lock(&registry->mutex);
    size_t retired = invalidate_locked(registry, range);
    pthread_mutex_unlock(&registry->mutex);

    return retired;
}

bool gles_record_coordinator_lookup(
    GlesRecordCoordinator *registry,
    GlesRecordCoordinatorKey key,
    GlesRecordRange *out_range)
{
    if (!registry || !registry->initialized ||
        !key.address || !key.generation)
        return false;

    pthread_mutex_lock(&registry->mutex);

    bool found = false;

    for (size_t i = 0; i < GLES_RECORD_COORDINATOR_CAPACITY; ++i) {
        const GlesRecordCoordinatorSlot *slot =
            &registry->slots[i];

        if (slot->occupied &&
            slot->key.address == key.address &&
            slot->key.generation == key.generation) {
            if (out_range)
                *out_range = slot->range;

            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&registry->mutex);
    return found;
}

size_t gles_record_coordinator_count(
    GlesRecordCoordinator *registry)
{
    if (!registry || !registry->initialized)
        return 0;

    pthread_mutex_lock(&registry->mutex);

    size_t count = 0;

    for (size_t i = 0; i < GLES_RECORD_COORDINATOR_CAPACITY; ++i)
        count += registry->slots[i].occupied ? 1u : 0u;

    pthread_mutex_unlock(&registry->mutex);
    return count;
}
