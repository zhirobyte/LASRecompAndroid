#include "lifecycle_processor.h"

#include <string.h>

bool gles_lifecycle_process(
    GlesLifecycleQueue *queue,
    GlesTextureCache *cache,
    GlesLifecycleProcessStats *stats
) {
    if (!queue || !cache || !stats)
        return false;

    memset(stats, 0, sizeof(*stats));

    /*
     * Once an event has been lost, the remaining queue
     * cannot be trusted to represent complete lifecycle
     * history. Discard everything and reset the cache.
     */
    if (queue->overflow) {
        gles_texture_cache_reset(cache);
        gles_lifecycle_init(queue);
        stats->recovered_overflow = true;
        return true;
    }

    GlesLifecycleEvent event;

    while (gles_lifecycle_pop(queue, &event)) {
        stats->events++;

        switch (event.type) {
        case GLES_LIFECYCLE_WRITE:
            stats->invalidated +=
                gles_texture_cache_invalidate_range(
                    cache, event.addr, event.size
                );
            break;

        case GLES_LIFECYCLE_TEXTURE_DESTROY:
            /*
             * release() compares pointer identities.
             * It does not dereference event.texture.
             */
            if (event.texture) {
                size_t before = 0;
                for (size_t i = 0;
                     i < GLES_TEXTURE_CACHE_CAPACITY; i++) {
                    if (cache->entries[i].texture == event.texture)
                        before++;
                }

                gles_texture_cache_release(
                    cache, event.texture
                );

                stats->retired += before;
            }
            break;

        case GLES_LIFECYCLE_POOL_DESTROY:
            stats->retired +=
                gles_texture_cache_release_pool(
                    cache, event.pool
                );
            break;

        default:
            /*
             * Unknown event: fail closed. A full reset
             * avoids retaining potentially stale entries.
             */
            gles_texture_cache_reset(cache);
            gles_lifecycle_init(queue);
            return false;
        }
    }

    return true;
}
