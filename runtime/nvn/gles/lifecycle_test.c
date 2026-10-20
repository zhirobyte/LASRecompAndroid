#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lifecycle.h"

static bool run_lifecycle_test(void)
{
    GlesLifecycleQueue queue;
    GlesLifecycleEvent event;

    NvnTexture texture = {0};
    NvnPool pool = {0};

    gles_lifecycle_init(&queue);

    bool empty =
        queue.count == 0 &&
        !queue.overflow &&
        !gles_lifecycle_pop(&queue, &event);

    fprintf(stderr, "[GLES-LIFECYCLE] EMPTY: %s\n",
            empty ? "PASS" : "FAIL");

    bool pushed =
        gles_lifecycle_push_write(&queue, 0x1000, 64) &&
        gles_lifecycle_push_texture_destroy(&queue, &texture) &&
        gles_lifecycle_push_pool_destroy(&queue, &pool) &&
        queue.count == 3;

    fprintf(stderr, "[GLES-LIFECYCLE] PUSH: %s\n",
            pushed ? "PASS" : "FAIL");

    bool first =
        gles_lifecycle_pop(&queue, &event) &&
        event.type == GLES_LIFECYCLE_WRITE &&
        event.addr == 0x1000 &&
        event.size == 64;

    bool second =
        gles_lifecycle_pop(&queue, &event) &&
        event.type == GLES_LIFECYCLE_TEXTURE_DESTROY &&
        event.texture == &texture;

    bool third =
        gles_lifecycle_pop(&queue, &event) &&
        event.type == GLES_LIFECYCLE_POOL_DESTROY &&
        event.pool == &pool;

    bool ordering = first && second && third && queue.count == 0;

    fprintf(stderr, "[GLES-LIFECYCLE] ORDERING: %s\n",
            ordering ? "PASS" : "FAIL");

    bool zero =
        gles_lifecycle_push_write(&queue, 0x1000, 0) &&
        queue.count == 0;

    fprintf(stderr, "[GLES-LIFECYCLE] ZERO WRITE: %s\n",
            zero ? "PASS" : "FAIL");

    bool capacity = true;

    for (size_t i = 0; i < GLES_LIFECYCLE_QUEUE_CAPACITY; i++) {
        if (!gles_lifecycle_push_write(&queue, 0x2000 + i, 1)) {
            capacity = false;
            break;
        }
    }

    capacity = capacity &&
        queue.count == GLES_LIFECYCLE_QUEUE_CAPACITY &&
        !queue.overflow;

    fprintf(stderr, "[GLES-LIFECYCLE] CAPACITY: %s\n",
            capacity ? "PASS" : "FAIL");

    bool overflow =
        !gles_lifecycle_push_write(&queue, 0x9000, 1) &&
        queue.overflow &&
        queue.count == GLES_LIFECYCLE_QUEUE_CAPACITY;

    fprintf(stderr, "[GLES-LIFECYCLE] OVERFLOW: %s\n",
            overflow ? "PASS" : "FAIL");

    bool fifo = true;

    for (size_t i = 0; i < GLES_LIFECYCLE_QUEUE_CAPACITY; i++) {
        if (!gles_lifecycle_pop(&queue, &event) ||
            event.type != GLES_LIFECYCLE_WRITE ||
            event.addr != 0x2000 + i ||
            event.size != 1) {
            fifo = false;
            break;
        }
    }

    fifo = fifo && queue.count == 0;

    fprintf(stderr, "[GLES-LIFECYCLE] FIFO: %s\n",
            fifo ? "PASS" : "FAIL");

    gles_lifecycle_init(&queue);

    bool reset =
        queue.count == 0 &&
        !queue.overflow &&
        !gles_lifecycle_pop(&queue, &event);

    fprintf(stderr, "[GLES-LIFECYCLE] RESET: %s\n",
            reset ? "PASS" : "FAIL");

    return empty && pushed && ordering && zero &&
           capacity && overflow && fifo && reset;
}

void gles_run_lifecycle_selftest(void)
{
    fprintf(stderr, "[GLES-LIFECYCLE] RESULT: %s\n",
            run_lifecycle_test() ? "PASS" : "FAIL");
}
