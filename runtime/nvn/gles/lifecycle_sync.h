#pragma once

#include <stdbool.h>
#include <pthread.h>

#include "lifecycle_processor.h"

typedef struct {
    pthread_mutex_t mutex;
    GlesLifecycleQueue pending;
    bool initialized;
} GlesLifecycleSync;

/*
 * Initialize before starting producers.
 * Destroy only after all producers and consumers stop.
 */
bool gles_lifecycle_sync_init(GlesLifecycleSync *sync);
void gles_lifecycle_sync_destroy(GlesLifecycleSync *sync);

bool gles_lifecycle_sync_write(
    GlesLifecycleSync *sync,
    uint64_t addr,
    uint64_t size
);

bool gles_lifecycle_sync_texture_destroy(
    GlesLifecycleSync *sync,
    const NvnTexture *texture
);

bool gles_lifecycle_sync_pool_destroy(
    GlesLifecycleSync *sync,
    const NvnPool *pool
);

/*
 * Consumer only: requires current GLES context.
 * The cache must not be concurrently accessed.
 */
bool gles_lifecycle_sync_process(
    GlesLifecycleSync *sync,
    GlesTextureCache *cache,
    GlesLifecycleProcessStats *stats
);
