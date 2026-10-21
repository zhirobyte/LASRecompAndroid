#include "record_rt_sidecar.h"

#include <string.h>

static bool same_record(
    GlesRecordCoordinatorKey a,
    GlesRecordCoordinatorKey b)
{
    return a.address == b.address &&
           a.generation == b.generation;
}

bool gles_record_rt_sidecar_init(
    GlesRecordRtSidecar *sidecar)
{
    if (!sidecar)
        return false;

    memset(sidecar, 0, sizeof(*sidecar));

    if (pthread_mutex_init(&sidecar->mutex, NULL) != 0)
        return false;

    sidecar->initialized = true;
    return true;
}

void gles_record_rt_sidecar_destroy(
    GlesRecordRtSidecar *sidecar)
{
    if (!sidecar || !sidecar->initialized)
        return;

    /* Caller must stop all concurrent users before destruction. */
    pthread_mutex_destroy(&sidecar->mutex);
    sidecar->initialized = false;
}

bool gles_record_rt_sidecar_insert(
    GlesRecordRtSidecar *sidecar,
    const GlesRecordRtMetadata *metadata)
{
    if (!sidecar || !sidecar->initialized ||
        !gles_record_rt_metadata_validate(metadata))
        return false;

    if (pthread_mutex_lock(&sidecar->mutex) != 0)
        return false;

    GlesRecordRtSidecarSlot *free_slot = NULL;
    bool duplicate = false;

    for (size_t i = 0;
         i < GLES_RECORD_RT_SIDECAR_CAPACITY;
         ++i) {
        GlesRecordRtSidecarSlot *slot = &sidecar->slots[i];

        if (slot->occupied &&
            same_record(
                slot->metadata.record_key,
                metadata->record_key)) {
            duplicate = true;
            break;
        }

        if (!slot->occupied && !free_slot)
            free_slot = slot;
    }

    bool inserted = false;

    if (!duplicate && free_slot) {
        free_slot->metadata = *metadata;
        free_slot->occupied = true;
        inserted = true;
    }

    pthread_mutex_unlock(&sidecar->mutex);
    return inserted;
}

bool gles_record_rt_sidecar_lookup(
    GlesRecordRtSidecar *sidecar,
    GlesRecordCoordinatorKey record_key,
    GlesRecordRtMetadata *out)
{
    if (!sidecar || !sidecar->initialized || !out ||
        !record_key.address || !record_key.generation)
        return false;

    if (pthread_mutex_lock(&sidecar->mutex) != 0)
        return false;

    bool found = false;

    for (size_t i = 0;
         i < GLES_RECORD_RT_SIDECAR_CAPACITY;
         ++i) {
        const GlesRecordRtSidecarSlot *slot = &sidecar->slots[i];

        if (slot->occupied &&
            same_record(slot->metadata.record_key, record_key)) {
            *out = slot->metadata;
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&sidecar->mutex);
    return found;
}

bool gles_record_rt_sidecar_retire(
    GlesRecordRtSidecar *sidecar,
    GlesRecordCoordinatorKey record_key)
{
    if (!sidecar || !sidecar->initialized ||
        !record_key.address || !record_key.generation)
        return false;

    if (pthread_mutex_lock(&sidecar->mutex) != 0)
        return false;

    bool retired = false;

    for (size_t i = 0;
         i < GLES_RECORD_RT_SIDECAR_CAPACITY;
         ++i) {
        GlesRecordRtSidecarSlot *slot = &sidecar->slots[i];

        if (slot->occupied &&
            same_record(slot->metadata.record_key, record_key)) {
            memset(&slot->metadata, 0, sizeof(slot->metadata));
            slot->occupied = false;
            retired = true;
            break;
        }
    }

    pthread_mutex_unlock(&sidecar->mutex);
    return retired;
}

size_t gles_record_rt_sidecar_count(
    GlesRecordRtSidecar *sidecar)
{
    if (!sidecar || !sidecar->initialized)
        return 0;

    if (pthread_mutex_lock(&sidecar->mutex) != 0)
        return 0;

    size_t count = 0;

    for (size_t i = 0;
         i < GLES_RECORD_RT_SIDECAR_CAPACITY;
         ++i)
        count += sidecar->slots[i].occupied ? 1u : 0u;

    pthread_mutex_unlock(&sidecar->mutex);
    return count;
}
