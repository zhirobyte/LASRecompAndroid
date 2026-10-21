#ifndef GLES_RECORD_COORDINATOR_H
#define GLES_RECORD_COORDINATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "record_range.h"

#define GLES_RECORD_COORDINATOR_CAPACITY 512

typedef struct {
    uintptr_t address;
    uint64_t generation;
} GlesRecordCoordinatorKey;

typedef struct {
    GlesRecordRange range;
    GlesRecordCoordinatorKey key;
    bool occupied;
} GlesRecordCoordinatorSlot;

typedef struct {
    pthread_mutex_t mutex;
    GlesRecordCoordinatorSlot slots[GLES_RECORD_COORDINATOR_CAPACITY];
    uint64_t next_generation;
    bool initialized;
} GlesRecordCoordinator;

/*
 * E167D3F_OVERLAP_COORDINATOR
 *
 * Caller must initialize before concurrent use.
 * Caller must stop all users before destruction.
 *
 * register(): invalidates overlapping records, then registers
 *             the new record with a unique generation.
 *
 * invalidate(): removes all records overlapping a write.
 *
 * lookup(): validates the exact address/generation pair.
 *
 * No command-memory dereferencing.
 *
 * IMPORTANT: All writes to tracked memory must be routed
 * through invalidate/register for identities to remain valid.
 */
bool gles_record_coordinator_init(GlesRecordCoordinator *registry);
void gles_record_coordinator_destroy(GlesRecordCoordinator *registry);

bool gles_record_coordinator_register(
    GlesRecordCoordinator *registry,
    GlesRecordRange range,
    GlesRecordCoordinatorKey *out);

size_t gles_record_coordinator_invalidate(
    GlesRecordCoordinator *registry,
    GlesRecordRange range);

bool gles_record_coordinator_lookup(
    GlesRecordCoordinator *registry,
    GlesRecordCoordinatorKey key,
    GlesRecordRange *out_range);

size_t gles_record_coordinator_count(
    GlesRecordCoordinator *registry);

#endif
