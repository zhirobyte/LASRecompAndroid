#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lifecycle.h"
#include "texture_cache.h"

typedef struct {
    size_t events;
    size_t invalidated;
    size_t retired;
    bool recovered_overflow;
} GlesLifecycleProcessStats;

/*
 * Diagnostic-only, single-threaded processor.
 * Must run with the owning GLES context current.
 * Producers must not modify the queue during processing.
 */
bool gles_lifecycle_process(
    GlesLifecycleQueue *queue,
    GlesTextureCache *cache,
    GlesLifecycleProcessStats *stats
);
