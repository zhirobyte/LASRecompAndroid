#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#define GLES_RECORD_IDENTITY_CAPACITY 512

/*
 * E167D3C_LIFECYCLE_CONTRACT
 *
 * init(): called before any concurrent access.
 * destroy(): called only after every user thread has stopped.
 *
 * Callers must never concurrently destroy and access this registry.
 * The mutex protects normal register/lookup/retire operations,
 * not the registry object's initialization or destruction.
 *
 * Retirement must precede command-memory address reuse.
 * That ordering is the responsibility of the future recorder bridge.
 */

typedef struct {
    uintptr_t record_address;
    uint64_t generation;
} GlesRecordIdentityKey;

typedef struct {
    GlesRecordIdentityKey key;
    bool occupied;
} GlesRecordIdentitySlot;

typedef struct {
    pthread_mutex_t mutex;
    GlesRecordIdentitySlot slots[GLES_RECORD_IDENTITY_CAPACITY];
    uint64_t next_generation;
    bool initialized;
} GlesRecordIdentityRegistry;

bool gles_record_identity_init(GlesRecordIdentityRegistry *registry);
void gles_record_identity_destroy(GlesRecordIdentityRegistry *registry);

bool gles_record_identity_register(
    GlesRecordIdentityRegistry *registry,
    uintptr_t record_address,
    GlesRecordIdentityKey *out);

bool gles_record_identity_lookup(
    GlesRecordIdentityRegistry *registry,
    GlesRecordIdentityKey key);

bool gles_record_identity_retire(
    GlesRecordIdentityRegistry *registry,
    GlesRecordIdentityKey key);

size_t gles_record_identity_count(
    GlesRecordIdentityRegistry *registry);
