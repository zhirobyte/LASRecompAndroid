#ifndef GLES_SUBMISSION_SNAPSHOT_H
#define GLES_SUBMISSION_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "record_rt_metadata.h"

#define GLES_SUBMISSION_SNAPSHOT_CAPACITY 128

typedef struct {
    GlesRecordRtMetadata metadata;
    bool occupied;
} GlesSubmissionSnapshotSlot;

typedef struct {
    pthread_mutex_t mutex;
    GlesSubmissionSnapshotSlot
        slots[GLES_SUBMISSION_SNAPSHOT_CAPACITY];
    uint64_t sequence;
    size_t count;
    bool initialized;
    bool active;
} GlesSubmissionSnapshot;

/*
 * Diagnostic-only value snapshot.
 *
 * No raw command-memory or texture-pointer dereferences.
 * This does not pin command memory or texture lifetime.
 *
 * Initialize before concurrent access.
 * Destroy only after all users have stopped.
 */

bool gles_submission_snapshot_init(
    GlesSubmissionSnapshot *snapshot);

void gles_submission_snapshot_destroy(
    GlesSubmissionSnapshot *snapshot);

bool gles_submission_snapshot_begin(
    GlesSubmissionSnapshot *snapshot,
    uint64_t sequence);

bool gles_submission_snapshot_insert(
    GlesSubmissionSnapshot *snapshot,
    const GlesRecordRtMetadata *metadata);

bool gles_submission_snapshot_lookup(
    GlesSubmissionSnapshot *snapshot,
    GlesRecordCoordinatorKey key,
    GlesRecordRtMetadata *out);

bool gles_submission_snapshot_retire(
    GlesSubmissionSnapshot *snapshot,
    uint64_t sequence);

size_t gles_submission_snapshot_count(
    GlesSubmissionSnapshot *snapshot);

#endif