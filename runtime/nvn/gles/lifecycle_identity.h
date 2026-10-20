#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "identity.h"
#include "texture_cache.h"

#define GLES_IDENTITY_EVENT_CAPACITY 128

typedef struct {
    GlesObjectIdentity identity;
} GlesIdentityDestroyEvent;

typedef struct {
    GlesIdentityDestroyEvent events[GLES_IDENTITY_EVENT_CAPACITY];
    size_t count;
    bool overflow;
} GlesIdentityEventQueue;

void gles_identity_events_init(GlesIdentityEventQueue *queue);

bool gles_identity_events_push_destroy(
    GlesIdentityEventQueue *queue,
    GlesObjectIdentity identity
);

bool gles_identity_events_process(
    GlesIdentityEventQueue *queue,
    GlesTextureCache *cache,
    size_t *retired
);
