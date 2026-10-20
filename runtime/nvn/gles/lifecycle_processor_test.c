#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lifecycle_processor.h"

static void seed(
    GlesTextureCache *cache,
    size_t index,
    const NvnTexture *texture,
    const NvnPool *pool,
    uint64_t addr,
    uint64_t size
) {
    GlesTextureCacheEntry *entry = &cache->entries[index];

    entry->texture = texture;
    entry->pool_identity = pool;
    entry->guest_addr = addr;
    entry->guest_size = size;

    /*
     * No GL name: these tests exercise metadata and
     * dispatch without allocating GPU resources.
     */
    entry->name = 0;
    entry->dirty = false;
}

static bool run_tests(void)
{
    GlesTextureCache cache;
    GlesLifecycleQueue queue;
    GlesLifecycleProcessStats stats;

    NvnTexture a = {0};
    NvnTexture b = {0};
    NvnTexture c = {0};

    NvnPool pool_a = {0};
    NvnPool pool_b = {0};

    gles_texture_cache_init(&cache);
    gles_lifecycle_init(&queue);

    seed(&cache, 0, &a, &pool_a, 0x1000, 64);
    seed(&cache, 1, &b, &pool_a, 0x2000, 64);
    seed(&cache, 2, &c, &pool_b, 0x3000, 64);

    bool pushed =
        gles_lifecycle_push_write(&queue, 0x1010, 4) &&
        gles_lifecycle_push_texture_destroy(&queue, &b);

    bool processed =
        pushed &&
        gles_lifecycle_process(&queue, &cache, &stats);

    bool write_ok =
        processed &&
        cache.entries[0].dirty &&
        !cache.entries[2].dirty &&
        stats.invalidated == 1;

    fprintf(stderr,
            "[GLES-LIFECYCLE-PROCESS] WRITE: %s\n",
            write_ok ? "PASS" : "FAIL");

    bool destroy_ok =
        processed &&
        cache.entries[1].texture == NULL &&
        cache.entries[0].texture == &a &&
        stats.retired == 1 &&
        stats.events == 2;

    fprintf(stderr,
            "[GLES-LIFECYCLE-PROCESS] TEXTURE DESTROY: %s\n",
            destroy_ok ? "PASS" : "FAIL");

    bool pool_pushed =
        gles_lifecycle_push_pool_destroy(&queue, &pool_a);

    bool pool_processed =
        pool_pushed &&
        gles_lifecycle_process(&queue, &cache, &stats);

    bool pool_ok =
        pool_processed &&
        cache.entries[0].texture == NULL &&
        cache.entries[2].texture == &c &&
        stats.retired == 1;

    fprintf(stderr,
            "[GLES-LIFECYCLE-PROCESS] POOL DESTROY: %s\n",
            pool_ok ? "PASS" : "FAIL");

    bool filled = true;

    for (size_t i = 0;
         i < GLES_LIFECYCLE_QUEUE_CAPACITY; i++) {
        if (!gles_lifecycle_push_write(
                &queue, 0x5000 + i, 1)) {
            filled = false;
            break;
        }
    }

    bool rejected =
        !gles_lifecycle_push_write(&queue, 0x9000, 1);

    bool recovered =
        filled &&
        rejected &&
        gles_lifecycle_process(&queue, &cache, &stats) &&
        stats.recovered_overflow &&
        queue.count == 0 &&
        !queue.overflow &&
        cache.entries[2].texture == NULL;

    fprintf(stderr,
            "[GLES-LIFECYCLE-PROCESS] OVERFLOW RESET: %s\n",
            recovered ? "PASS" : "FAIL");

    seed(&cache, 0, &a, &pool_a, 0x1000, 64);

    GlesLifecycleEvent invalid = {0};
    invalid.type = (GlesLifecycleEventType)99;
    queue.events[0] = invalid;
    queue.count = 1;

    bool unknown =
        !gles_lifecycle_process(&queue, &cache, &stats) &&
        cache.entries[0].texture == NULL &&
        queue.count == 0;

    fprintf(stderr,
            "[GLES-LIFECYCLE-PROCESS] UNKNOWN EVENT: %s\n",
            unknown ? "PASS" : "FAIL");

    gles_texture_cache_reset(&cache);

    return write_ok && destroy_ok && pool_ok &&
           recovered && unknown;
}

void gles_run_lifecycle_processor_selftest(void)
{
    fprintf(stderr,
            "[GLES-LIFECYCLE-PROCESS] RESULT: %s\n",
            run_tests() ? "PASS" : "FAIL");
}
