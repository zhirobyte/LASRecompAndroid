#ifndef GLES_RECORD_RT_SIDECAR_H
#define GLES_RECORD_RT_SIDECAR_H

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#include "record_rt_metadata.h"

#define GLES_RECORD_RT_SIDECAR_CAPACITY 512

typedef struct {
    GlesRecordRtMetadata metadata;
    bool occupied;
} GlesRecordRtSidecarSlot;

typedef struct {
    pthread_mutex_t mutex;
    GlesRecordRtSidecarSlot slots[GLES_RECORD_RT_SIDECAR_CAPACITY];
    bool initialized;
} GlesRecordRtSidecar;

bool gles_record_rt_sidecar_init(
    GlesRecordRtSidecar *sidecar);

void gles_record_rt_sidecar_destroy(
    GlesRecordRtSidecar *sidecar);

bool gles_record_rt_sidecar_insert(
    GlesRecordRtSidecar *sidecar,
    const GlesRecordRtMetadata *metadata);

bool gles_record_rt_sidecar_lookup(
    GlesRecordRtSidecar *sidecar,
    GlesRecordCoordinatorKey record_key,
    GlesRecordRtMetadata *out);

bool gles_record_rt_sidecar_retire(
    GlesRecordRtSidecar *sidecar,
    GlesRecordCoordinatorKey record_key);

size_t gles_record_rt_sidecar_count(
    GlesRecordRtSidecar *sidecar);

#endif
