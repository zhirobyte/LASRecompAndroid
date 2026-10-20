#include "lifecycle_sync.h"

#include <string.h>

bool gles_lifecycle_sync_init(GlesLifecycleSync *sync)
{
    if (!sync)
        return false;

    memset(sync, 0, sizeof(*sync));

    if (pthread_mutex_init(&sync->mutex, NULL) != 0)
        return false;

    gles_lifecycle_init(&sync->pending);
    sync->initialized = true;
    return true;
}

void gles_lifecycle_sync_destroy(GlesLifecycleSync *sync)
{
    if (!sync || !sync->initialized)
        return;

    /*
     * Caller must first join all producer threads.
     * No other thread may be using this object.
     */
    pthread_mutex_destroy(&sync->mutex);
    sync->initialized = false;
}

static bool submit(
    GlesLifecycleSync *sync,
    GlesLifecycleEvent event
) {
    if (!sync || !sync->initialized)
        return false;

    if (pthread_mutex_lock(&sync->mutex) != 0)
        return false;

    GlesLifecycleQueue *queue = &sync->pending;
    bool result = false;

    /*
     * Keep the overflow flag sticky until the consumer
     * takes a snapshot and resets the cache.
     */
    if (!queue->overflow) {
        if (queue->count < GLES_LIFECYCLE_QUEUE_CAPACITY) {
            queue->events[queue->count++] = event;
            result = true;
        } else {
            queue->overflow = true;
        }
    }

    pthread_mutex_unlock(&sync->mutex);
    return result;
}

bool gles_lifecycle_sync_write(
    GlesLifecycleSync *sync,
    uint64_t addr,
    uint64_t size
) {
    if (!sync || !sync->initialized)
        return false;

    if (!size)
        return true;

    GlesLifecycleEvent event = {0};
    event.type = GLES_LIFECYCLE_WRITE;
    event.addr = addr;
    event.size = size;

    return submit(sync, event);
}

bool gles_lifecycle_sync_texture_destroy(
    GlesLifecycleSync *sync,
    const NvnTexture *texture
) {
    if (!texture)
        return false;

    GlesLifecycleEvent event = {0};
    event.type = GLES_LIFECYCLE_TEXTURE_DESTROY;
    event.texture = texture;

    return submit(sync, event);
}

bool gles_lifecycle_sync_pool_destroy(
    GlesLifecycleSync *sync,
    const NvnPool *pool
) {
    if (!pool)
        return false;

    GlesLifecycleEvent event = {0};
    event.type = GLES_LIFECYCLE_POOL_DESTROY;
    event.pool = pool;

    return submit(sync, event);
}

bool gles_lifecycle_sync_process(
    GlesLifecycleSync *sync,
    GlesTextureCache *cache,
    GlesLifecycleProcessStats *stats
) {
    if (!sync || !sync->initialized || !cache || !stats)
        return false;

    /*
     * Take a complete batch while holding the mutex.
     * Do not perform any GL operations while locked.
     */
    GlesLifecycleQueue batch;

    if (pthread_mutex_lock(&sync->mutex) != 0)
        return false;

    batch = sync->pending;
    gles_lifecycle_init(&sync->pending);

    pthread_mutex_unlock(&sync->mutex);

    /*
     * The original processor handles overflow by
     * resetting the cache and discarding this batch.
     */
    return gles_lifecycle_process(&batch, cache, stats);
}
