#include <stdbool.h>
#include <stdio.h>

#include "lifecycle_identity.h"

static bool run_tests(void)
{
    GlesTextureCache cache;
    GlesIdentityRegistry registry;
    GlesIdentityEventQueue queue;

    gles_texture_cache_init(&cache);
    gles_identity_init(&registry);
    gles_identity_events_init(&queue);

    NvnTexture texture = {0};

    GlesObjectIdentity first = {0};
    GlesObjectIdentity second = {0};

    if (!gles_identity_register(&registry, &texture, &first))
        return false;

    cache.entries[0].texture = &texture;
    cache.entries[0].identity = first;

    /*
     * Capture the old identity before destruction.
     */
    bool queued =
        gles_identity_events_push_destroy(&queue, first);

    bool retired_first =
        gles_identity_retire(&registry, first);

    /*
     * Simulate allocation of a replacement at the
     * same memory address.
     */
    bool registered_second =
        gles_identity_register(&registry, &texture, &second);

    cache.entries[0].texture = &texture;
    cache.entries[0].identity = second;

    size_t retired = 999;

    bool processed =
        gles_identity_events_process(&queue, &cache, &retired);

    bool stale_safe =
        queued &&
        retired_first &&
        registered_second &&
        first.generation != second.generation &&
        processed &&
        retired == 0 &&
        cache.entries[0].texture == &texture &&
        cache.entries[0].identity.generation == second.generation;

    fprintf(stderr,
            "[GLES-LIFECYCLE-ID] STALE DESTROY: %s\n",
            stale_safe ? "PASS" : "FAIL");

    bool queued_second =
        gles_identity_events_push_destroy(&queue, second);

    bool processed_second =
        gles_identity_events_process(&queue, &cache, &retired);

    bool current_retired =
        queued_second &&
        processed_second &&
        retired == 1 &&
        cache.entries[0].texture == NULL;

    fprintf(stderr,
            "[GLES-LIFECYCLE-ID] CURRENT DESTROY: %s\n",
            current_retired ? "PASS" : "FAIL");

    bool filled = true;

    for (size_t i = 0; i < GLES_IDENTITY_EVENT_CAPACITY; i++) {
        if (!gles_identity_events_push_destroy(&queue, second)) {
            filled = false;
            break;
        }
    }

    bool rejected =
        !gles_identity_events_push_destroy(&queue, second);

    bool recovered =
        gles_identity_events_process(&queue, &cache, &retired) &&
        queue.count == 0 &&
        !queue.overflow;

    bool overflow = filled && rejected && recovered;

    fprintf(stderr,
            "[GLES-LIFECYCLE-ID] OVERFLOW: %s\n",
            overflow ? "PASS" : "FAIL");

    return stale_safe && current_retired && overflow;
}

void gles_run_lifecycle_identity_selftest(void)
{
    fprintf(stderr,
            "[GLES-LIFECYCLE-ID] RESULT: %s\n",
            run_tests() ? "PASS" : "FAIL");
}
