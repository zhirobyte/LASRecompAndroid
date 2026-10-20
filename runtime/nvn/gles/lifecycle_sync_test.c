#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lifecycle_sync.h"

typedef struct {
    GlesLifecycleSync *sync;
    uint64_t address;
    bool submitted;
} ProducerArgs;

static void *producer(void *argument)
{
    ProducerArgs *args = (ProducerArgs *)argument;

    args->submitted = gles_lifecycle_sync_write(
        args->sync, args->address, 8
    );

    return NULL;
}

static bool run_tests(void)
{
    GlesLifecycleSync sync = {0};
    GlesTextureCache cache;
    GlesLifecycleProcessStats stats;

    gles_texture_cache_init(&cache);

    bool initialized = gles_lifecycle_sync_init(&sync);

    if (!initialized) {
        fprintf(stderr,
                "[GLES-LIFECYCLE-SYNC] RESULT: FAIL\n");
        return false;
    }

    /*
     * Four producer threads submit independent writes.
     * No cache operations occur on producer threads.
     */
    pthread_t threads[4];
    ProducerArgs args[4] = {0};
    size_t started = 0;
    bool created = true;

    for (size_t i = 0; i < 4; i++) {
        args[i].sync = &sync;
        args[i].address = 0x1000 + i * 0x100;

        if (pthread_create(
                &threads[i], NULL, producer, &args[i]) != 0) {
            created = false;
            break;
        }

        started++;
    }

    for (size_t i = 0; i < started; i++)
        pthread_join(threads[i], NULL);

    bool submissions = created && started == 4;

    for (size_t i = 0; i < started; i++)
        submissions = submissions && args[i].submitted;

    bool processed =
        gles_lifecycle_sync_process(&sync, &cache, &stats);

    bool concurrent =
        submissions &&
        processed &&
        stats.events == 4 &&
        !stats.recovered_overflow;

    fprintf(stderr,
            "[GLES-LIFECYCLE-SYNC] PRODUCERS: %s\n",
            concurrent ? "PASS" : "FAIL");

    /*
     * Verify the queue was drained.
     */
    bool drained =
        gles_lifecycle_sync_process(&sync, &cache, &stats) &&
        stats.events == 0;

    fprintf(stderr,
            "[GLES-LIFECYCLE-SYNC] DRAIN: %s\n",
            drained ? "PASS" : "FAIL");

    /*
     * Fill the queue and submit one extra event.
     * The overflow must survive until processing.
     */
    bool filled = true;

    for (size_t i = 0;
         i < GLES_LIFECYCLE_QUEUE_CAPACITY; i++) {
        if (!gles_lifecycle_sync_write(
                &sync, 0x4000 + i, 1)) {
            filled = false;
            break;
        }
    }

    bool rejected =
        !gles_lifecycle_sync_write(&sync, 0x9000, 1);

    bool overflow =
        filled &&
        rejected &&
        gles_lifecycle_sync_process(&sync, &cache, &stats) &&
        stats.recovered_overflow;

    fprintf(stderr,
            "[GLES-LIFECYCLE-SYNC] OVERFLOW: %s\n",
            overflow ? "PASS" : "FAIL");

    /*
     * A recovered queue must accept new events.
     */
    bool reused =
        gles_lifecycle_sync_write(&sync, 0xA000, 16) &&
        gles_lifecycle_sync_process(&sync, &cache, &stats) &&
        stats.events == 1 &&
        !stats.recovered_overflow;

    fprintf(stderr,
            "[GLES-LIFECYCLE-SYNC] RECOVERY: %s\n",
            reused ? "PASS" : "FAIL");

    gles_lifecycle_sync_destroy(&sync);
    gles_texture_cache_reset(&cache);

    return concurrent && drained && overflow && reused;
}

void gles_run_lifecycle_sync_selftest(void)
{
    fprintf(stderr,
            "[GLES-LIFECYCLE-SYNC] RESULT: %s\n",
            run_tests() ? "PASS" : "FAIL");
}
