#include "lifecycle.h"

#include <string.h>

void gles_lifecycle_init(GlesLifecycleQueue *queue)
{
    if (queue)
        memset(queue, 0, sizeof(*queue));
}

static bool push(
    GlesLifecycleQueue *queue,
    GlesLifecycleEvent event
) {
    if (!queue)
        return false;

    if (queue->count >= GLES_LIFECYCLE_QUEUE_CAPACITY) {
        queue->overflow = true;
        return false;
    }

    queue->events[queue->count++] = event;
    return true;
}

bool gles_lifecycle_push_write(
    GlesLifecycleQueue *queue,
    uint64_t addr,
    uint64_t size
) {
    if (!size)
        return true;

    GlesLifecycleEvent event = {0};
    event.type = GLES_LIFECYCLE_WRITE;
    event.addr = addr;
    event.size = size;

    return push(queue, event);
}

bool gles_lifecycle_push_texture_destroy(
    GlesLifecycleQueue *queue,
    const NvnTexture *texture
) {
    if (!texture)
        return false;

    GlesLifecycleEvent event = {0};
    event.type = GLES_LIFECYCLE_TEXTURE_DESTROY;
    event.texture = texture;

    return push(queue, event);
}

bool gles_lifecycle_push_pool_destroy(
    GlesLifecycleQueue *queue,
    const NvnPool *pool
) {
    if (!pool)
        return false;

    GlesLifecycleEvent event = {0};
    event.type = GLES_LIFECYCLE_POOL_DESTROY;
    event.pool = pool;

    return push(queue, event);
}

bool gles_lifecycle_pop(
    GlesLifecycleQueue *queue,
    GlesLifecycleEvent *event
) {
    if (!queue || !event || !queue->count)
        return false;

    *event = queue->events[0];

    queue->count--;

    if (queue->count) {
        memmove(
            &queue->events[0],
            &queue->events[1],
            queue->count * sizeof(queue->events[0])
        );
    }

    return true;
}
