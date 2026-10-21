#include "submission_snapshot.h"

#include <string.h>

bool gles_submission_snapshot_init(
    GlesSubmissionSnapshot *snapshot)
{
    if (!snapshot)
        return false;

    memset(snapshot, 0, sizeof(*snapshot));

    if (pthread_mutex_init(&snapshot->mutex, NULL) != 0)
        return false;

    snapshot->initialized = true;
    return true;
}

void gles_submission_snapshot_destroy(
    GlesSubmissionSnapshot *snapshot)
{
    if (!snapshot || !snapshot->initialized)
        return;

    pthread_mutex_destroy(&snapshot->mutex);
    snapshot->initialized = false;
}

bool gles_submission_snapshot_begin(
    GlesSubmissionSnapshot *snapshot,
    uint64_t sequence)
{
    if (!snapshot || !snapshot->initialized || !sequence)
        return false;

    if (pthread_mutex_lock(&snapshot->mutex) != 0)
        return false;

    if (snapshot->active) {
        pthread_mutex_unlock(&snapshot->mutex);
        return false;
    }

    memset(snapshot->slots, 0, sizeof(snapshot->slots));
    snapshot->count = 0;
    snapshot->sequence = sequence;
    snapshot->active = true;

    pthread_mutex_unlock(&snapshot->mutex);
    return true;
}

bool gles_submission_snapshot_insert(
    GlesSubmissionSnapshot *snapshot,
    const GlesRecordRtMetadata *metadata)
{
    if (!snapshot || !snapshot->initialized ||
        !gles_record_rt_metadata_validate(metadata))
        return false;

    if (pthread_mutex_lock(&snapshot->mutex) != 0)
        return false;

    if (!snapshot->active ||
        snapshot->count >= GLES_SUBMISSION_SNAPSHOT_CAPACITY) {
        pthread_mutex_unlock(&snapshot->mutex);
        return false;
    }

    GlesSubmissionSnapshotSlot *free_slot = NULL;

    for (size_t i = 0;
         i < GLES_SUBMISSION_SNAPSHOT_CAPACITY;
         ++i) {
        GlesSubmissionSnapshotSlot *slot =
            &snapshot->slots[i];

        if (slot->occupied) {
            /*
             * Reject address reuse even when generations differ.
             * A submission cannot contain ambiguous identities.
             */
            if (slot->metadata.record_key.address ==
                metadata->record_key.address) {
                pthread_mutex_unlock(&snapshot->mutex);
                return false;
            }
        } else if (!free_slot) {
            free_slot = slot;
        }
    }

    if (!free_slot) {
        pthread_mutex_unlock(&snapshot->mutex);
        return false;
    }

    free_slot->metadata = *metadata;
    free_slot->occupied = true;
    snapshot->count++;

    pthread_mutex_unlock(&snapshot->mutex);
    return true;
}

bool gles_submission_snapshot_lookup(
    GlesSubmissionSnapshot *snapshot,
    GlesRecordCoordinatorKey key,
    GlesRecordRtMetadata *out)
{
    if (!snapshot || !snapshot->initialized || !out ||
        !key.address || !key.generation)
        return false;

    if (pthread_mutex_lock(&snapshot->mutex) != 0)
        return false;

    if (!snapshot->active) {
        pthread_mutex_unlock(&snapshot->mutex);
        return false;
    }

    bool found = false;

    for (size_t i = 0;
         i < GLES_SUBMISSION_SNAPSHOT_CAPACITY;
         ++i) {
        GlesSubmissionSnapshotSlot *slot =
            &snapshot->slots[i];

        if (!slot->occupied)
            continue;

        if (slot->metadata.record_key.address == key.address &&
            slot->metadata.record_key.generation ==
                key.generation) {
            *out = slot->metadata;
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&snapshot->mutex);
    return found;
}

bool gles_submission_snapshot_retire(
    GlesSubmissionSnapshot *snapshot,
    uint64_t sequence)
{
    if (!snapshot || !snapshot->initialized || !sequence)
        return false;

    if (pthread_mutex_lock(&snapshot->mutex) != 0)
        return false;

    if (!snapshot->active ||
        snapshot->sequence != sequence) {
        pthread_mutex_unlock(&snapshot->mutex);
        return false;
    }

    memset(snapshot->slots, 0, sizeof(snapshot->slots));
    snapshot->count = 0;
    snapshot->sequence = 0;
    snapshot->active = false;

    pthread_mutex_unlock(&snapshot->mutex);
    return true;
}

size_t gles_submission_snapshot_count(
    GlesSubmissionSnapshot *snapshot)
{
    if (!snapshot || !snapshot->initialized)
        return 0;

    if (pthread_mutex_lock(&snapshot->mutex) != 0)
        return 0;

    size_t count = snapshot->active
        ? snapshot->count
        : 0;

    pthread_mutex_unlock(&snapshot->mutex);
    return count;
}