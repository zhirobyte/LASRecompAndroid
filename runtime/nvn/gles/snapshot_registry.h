#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "texture_snapshot.h"

#define GLES_SNAPSHOT_REGISTRY_CAPACITY 256

typedef struct {
    GlesTextureSnapshot snapshot;
    bool occupied;
} GlesSnapshotRegistrySlot;

typedef struct {
    pthread_mutex_t mutex;
    GlesSnapshotRegistrySlot slots[GLES_SNAPSHOT_REGISTRY_CAPACITY];
    uint64_t next_generation;
    bool initialized;
} GlesSnapshotRegistry;

/*
 * Caller must ensure no other thread is using the registry
 * when init/destroy are called.
 */
bool gles_snapshot_registry_init(GlesSnapshotRegistry *registry);
void gles_snapshot_registry_destroy(GlesSnapshotRegistry *registry);

/*
 * The input is an already-copied value, not a live texture pointer.
 * Registration assigns a new generation and returns its complete key.
 */
bool gles_snapshot_registry_register(
    GlesSnapshotRegistry *registry,
    const GlesTextureSnapshot *metadata,
    GlesTextureSnapshotKey *key);

/* Copies metadata under the mutex. Never dereferences pointer_value. */
bool gles_snapshot_registry_lookup(
    GlesSnapshotRegistry *registry,
    GlesTextureSnapshotKey key,
    GlesTextureSnapshot *out);

bool gles_snapshot_registry_retire(
    GlesSnapshotRegistry *registry,
    GlesTextureSnapshotKey key);

size_t gles_snapshot_registry_count(GlesSnapshotRegistry *registry);
