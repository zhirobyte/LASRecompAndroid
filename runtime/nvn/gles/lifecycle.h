#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../nvn.h"

#define GLES_LIFECYCLE_QUEUE_CAPACITY 128

typedef enum {
    GLES_LIFECYCLE_WRITE = 1,
    GLES_LIFECYCLE_TEXTURE_DESTROY = 2,
    GLES_LIFECYCLE_POOL_DESTROY = 3
} GlesLifecycleEventType;

typedef struct {
    GlesLifecycleEventType type;
    uint64_t addr;
    uint64_t size;
    const NvnTexture *texture;
    const NvnPool *pool;
} GlesLifecycleEvent;

typedef struct {
    GlesLifecycleEvent events[GLES_LIFECYCLE_QUEUE_CAPACITY];
    size_t count;
    bool overflow;
} GlesLifecycleQueue;

void gles_lifecycle_init(GlesLifecycleQueue *queue);

bool gles_lifecycle_push_write(
    GlesLifecycleQueue *queue,
    uint64_t addr,
    uint64_t size
);

bool gles_lifecycle_push_texture_destroy(
    GlesLifecycleQueue *queue,
    const NvnTexture *texture
);

bool gles_lifecycle_push_pool_destroy(
    GlesLifecycleQueue *queue,
    const NvnPool *pool
);

bool gles_lifecycle_pop(
    GlesLifecycleQueue *queue,
    GlesLifecycleEvent *event
);
