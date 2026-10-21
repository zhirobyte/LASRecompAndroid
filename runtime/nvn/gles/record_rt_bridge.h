#ifndef GLES_RECORD_RT_BRIDGE_H
#define GLES_RECORD_RT_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "record_coordinator.h"
#include "record_rt_metadata.h"
#include "record_rt_sidecar.h"
#include "submission_snapshot.h"

typedef struct {
    pthread_mutex_t mutex;
    GlesRecordCoordinator records;
    GlesRecordRtSidecar sidecar;
    bool initialized;
} GlesRecordRtBridge;

/*
 * E167D3G4JR2:
 * Value-only, diagnostic-only RT metadata bridge.
 *
 * Caller must initialize before concurrent use and stop all
 * users before destruction.
 *
 * All bridge operations are serialized by the outer mutex.
 * No command-memory or texture-pointer dereferences.
 *
 * This does not pin guest command memory or texture lifetime.
 */
bool gles_record_rt_bridge_init(GlesRecordRtBridge *bridge);
void gles_record_rt_bridge_destroy(GlesRecordRtBridge *bridge);

bool gles_record_rt_bridge_register(
    GlesRecordRtBridge *bridge,
    GlesRecordRange range,
    const GlesRecordRtMetadata *input,
    GlesRecordCoordinatorKey *out_key);

size_t gles_record_rt_bridge_invalidate(
    GlesRecordRtBridge *bridge,
    GlesRecordRange written_range);

bool gles_record_rt_bridge_lookup(
    GlesRecordRtBridge *bridge,
    GlesRecordCoordinatorKey key,
    GlesRecordRtMetadata *out);

size_t gles_record_rt_bridge_count(GlesRecordRtBridge *bridge);


/*
 * R8D-1 diagnostic-only snapshot copy.
 *
 * The caller must begin an empty snapshot before calling.
 * The bridge mutex protects enumeration and copying.
 *
 * This does not pin command memory or texture lifetimes.
 */
bool gles_record_rt_bridge_snapshot_copy(
    GlesRecordRtBridge *bridge,
    GlesSubmissionSnapshot *snapshot,
    size_t *copied);

/*
 * R8D-3: copy only explicitly supplied record generations.
 *
 * The caller must provide original generation-aware keys.
 * No address-only generation reconstruction is permitted.
 *
 * The destination must be an active, empty snapshot.
 * Failure may leave a partial snapshot; caller must retire it.
 *
 * Diagnostic only: does not pin guest memory or textures.
 */
bool gles_record_rt_bridge_snapshot_copy_keys(
    GlesRecordRtBridge *bridge,
    GlesSubmissionSnapshot *snapshot,
    const GlesRecordCoordinatorKey *keys,
    size_t key_count,
    size_t *copied);
#endif