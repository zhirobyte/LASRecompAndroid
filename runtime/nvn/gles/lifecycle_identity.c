#include "lifecycle_identity.h"

#include <string.h>

void gles_identity_events_init(GlesIdentityEventQueue *queue)
{
    if (queue)
        memset(queue, 0, sizeof(*queue));
}

bool gles_identity_events_push_destroy(
    GlesIdentityEventQueue *queue,
    GlesObjectIdentity identity
) {
    if (!queue || !identity.pointer || !identity.generation)
        return false;

    if (queue->overflow)
        return false;

    if (queue->count >= GLES_IDENTITY_EVENT_CAPACITY) {
        queue->overflow = true;
        return false;
    }

    queue->events[queue->count++].identity = identity;
    return true;
}

bool gles_identity_events_process(
    GlesIdentityEventQueue *queue,
    GlesTextureCache *cache,
    size_t *retired
) {
    if (!queue || !cache || !retired)
        return false;

    *retired = 0;

    /*
     * An overflow means at least one destruction event
     * was lost. Reset the cache conservatively.
     */
    if (queue->overflow) {
        gles_texture_cache_reset(cache);
        gles_identity_events_init(queue);
        return true;
    }

    for (size_t i = 0; i < queue->count; i++) {
        if (gles_texture_cache_release_identity(
                cache, queue->events[i].identity)) {
            (*retired)++;
        }
    }

    gles_identity_events_init(queue);
    return true;
}
