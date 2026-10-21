#include "record_rt_bridge.h"

#include <string.h>

static void retire_overlaps(
    GlesRecordRtBridge *bridge,
    GlesRecordRange range)
{
    /*
     * The bridge mutex is held.
     * Retire sidecar entries before coordinator invalidation.
     */
    for (size_t i = 0; i < GLES_RECORD_COORDINATOR_CAPACITY; ++i) {
        const GlesRecordCoordinatorSlot *slot =
            &bridge->records.slots[i];

        if (!slot->occupied ||
            !gles_record_range_overlaps(slot->range, range))
            continue;

        gles_record_rt_sidecar_retire(
            &bridge->sidecar, slot->key);
    }
}

bool gles_record_rt_bridge_init(GlesRecordRtBridge *bridge)
{
    if (!bridge)
        return false;

    memset(bridge, 0, sizeof(*bridge));

    if (pthread_mutex_init(&bridge->mutex, NULL) != 0)
        return false;

    if (!gles_record_coordinator_init(&bridge->records)) {
        pthread_mutex_destroy(&bridge->mutex);
        return false;
    }

    if (!gles_record_rt_sidecar_init(&bridge->sidecar)) {
        gles_record_coordinator_destroy(&bridge->records);
        pthread_mutex_destroy(&bridge->mutex);
        return false;
    }

    bridge->initialized = true;
    return true;
}

void gles_record_rt_bridge_destroy(GlesRecordRtBridge *bridge)
{
    if (!bridge || !bridge->initialized)
        return;

    gles_record_rt_sidecar_destroy(&bridge->sidecar);
    gles_record_coordinator_destroy(&bridge->records);
    pthread_mutex_destroy(&bridge->mutex);
    bridge->initialized = false;
}

bool gles_record_rt_bridge_register(
    GlesRecordRtBridge *bridge,
    GlesRecordRange range,
    const GlesRecordRtMetadata *input,
    GlesRecordCoordinatorKey *out_key)
{
    if (!bridge || !bridge->initialized || !input || !out_key ||
        !gles_record_range_valid(range))
        return false;

    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return false;

    /*
     * Invalidate overlap even if registration subsequently fails.
     * A failed registration must never preserve stale metadata.
     */
    retire_overlaps(bridge, range);
    gles_record_coordinator_invalidate(&bridge->records, range);

    GlesRecordCoordinatorKey key = {0};
    bool ok = gles_record_coordinator_register(
        &bridge->records, range, &key);

    if (ok) {
        GlesRecordRtMetadata metadata = *input;
        metadata.record_key = key;

        ok = gles_record_rt_sidecar_insert(
            &bridge->sidecar, &metadata);

        if (!ok)
            gles_record_coordinator_invalidate(
                &bridge->records, range);
        else
            *out_key = key;
    }

    pthread_mutex_unlock(&bridge->mutex);
    return ok;
}

size_t gles_record_rt_bridge_invalidate(
    GlesRecordRtBridge *bridge,
    GlesRecordRange written_range)
{
    if (!bridge || !bridge->initialized ||
        !gles_record_range_valid(written_range))
        return 0;

    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return 0;

    retire_overlaps(bridge, written_range);

    size_t count = gles_record_coordinator_invalidate(
        &bridge->records, written_range);

    pthread_mutex_unlock(&bridge->mutex);
    return count;
}

bool gles_record_rt_bridge_lookup(
    GlesRecordRtBridge *bridge,
    GlesRecordCoordinatorKey key,
    GlesRecordRtMetadata *out)
{
    if (!bridge || !bridge->initialized || !out)
        return false;

    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return false;

    bool ok = gles_record_coordinator_lookup(
        &bridge->records, key, NULL);

    if (ok)
        ok = gles_record_rt_sidecar_lookup(
            &bridge->sidecar, key, out);

    pthread_mutex_unlock(&bridge->mutex);
    return ok;
}


/*
 * R8D-1: diagnostic-only, value-only submission snapshot.
 * Must not be used as proof that command memory is immutable.
 */
bool gles_record_rt_bridge_snapshot_copy(
    GlesRecordRtBridge *bridge,
    GlesSubmissionSnapshot *snapshot,
    size_t *copied)
{
    if (copied)
        *copied = 0;

    if (!bridge || !bridge->initialized ||
        !snapshot || !copied)
        return false;

    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return false;

    size_t count = 0;
    bool ok = true;

    for (size_t i = 0;
         i < GLES_RECORD_RT_SIDECAR_CAPACITY;
         ++i) {
        const GlesRecordRtSidecarSlot *slot =
            &bridge->sidecar.slots[i];

        if (!slot->occupied)
            continue;

        GlesRecordRtMetadata metadata = slot->metadata;

        if (!gles_record_coordinator_lookup(
                &bridge->records,
                metadata.record_key,
                NULL)) {
            ok = false;
            break;
        }

        if (!gles_submission_snapshot_insert(
                snapshot, &metadata)) {
            ok = false;
            break;
        }

        ++count;
    }

    pthread_mutex_unlock(&bridge->mutex);

    *copied = count;
    return ok;
}

/*
 * R8D-3: exact-key, diagnostic-only submission snapshot.
 *
 * The bridge mutex serializes registration, invalidation,
 * and lookup while metadata is copied.
 */
bool gles_record_rt_bridge_snapshot_copy_keys(
    GlesRecordRtBridge *bridge,
    GlesSubmissionSnapshot *snapshot,
    const GlesRecordCoordinatorKey *keys,
    size_t key_count,
    size_t *copied)
{
    if (copied)
        *copied = 0;

    if (!bridge || !bridge->initialized ||
        !snapshot || !copied ||
        (!keys && key_count != 0) ||
        key_count > GLES_SUBMISSION_SNAPSHOT_CAPACITY)
        return false;

    /*
     * A zero-length key set is valid: the submitted command
     * list might contain no render-target operations.
     */
    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return false;

    bool ok = true;
    size_t count = 0;

    for (size_t i = 0; i < key_count; ++i) {
        const GlesRecordCoordinatorKey key = keys[i];
        GlesRecordRtMetadata metadata = {0};

        if (!key.address || !key.generation) {
            ok = false;
            break;
        }

        if (!gles_record_coordinator_lookup(
                &bridge->records, key, NULL)) {
            ok = false;
            break;
        }

        if (!gles_record_rt_sidecar_lookup(
                &bridge->sidecar, key, &metadata)) {
            ok = false;
            break;
        }

        if (!gles_submission_snapshot_insert(
                snapshot, &metadata)) {
            ok = false;
            break;
        }

        ++count;
    }

    pthread_mutex_unlock(&bridge->mutex);

    *copied = count;
    return ok;
}
size_t gles_record_rt_bridge_count(GlesRecordRtBridge *bridge)
{
    if (!bridge || !bridge->initialized)
        return 0;

    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return 0;

    size_t count = gles_record_coordinator_count(
        &bridge->records);

    pthread_mutex_unlock(&bridge->mutex);
    return count;
}