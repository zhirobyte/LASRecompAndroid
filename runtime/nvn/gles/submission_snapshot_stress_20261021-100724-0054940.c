#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

#include "submission_snapshot.h"

#define THREADS 4
#define ITERATIONS 2000

static GlesSubmissionSnapshot snapshot;
static atomic_int failures;

static GlesRecordRtMetadata make_metadata(
    uintptr_t address, uint64_t generation)
{
    GlesRecordRtMetadata m = {0};
    m.valid = true;
    m.record_key.address = address;
    m.record_key.generation = generation;
    return m;
}

static void *reader(void *arg)
{
    uintptr_t id = (uintptr_t)arg;

    for (int i = 0; i < ITERATIONS; ++i) {
        GlesRecordCoordinatorKey key = {
            .address = 0x1000 + id * 0x100,
            .generation = id + 1
        };

        GlesRecordRtMetadata out = {0};

        if (!gles_submission_snapshot_lookup(
                &snapshot, key, &out)) {
            atomic_fetch_add(&failures, 1);
        } else if (out.record_key.address != key.address ||
                   out.record_key.generation != key.generation) {
            atomic_fetch_add(&failures, 1);
        }
    }

    return NULL;
}

int main(void)
{
    if (!gles_submission_snapshot_init(&snapshot))
        return 2;

    if (!gles_submission_snapshot_begin(&snapshot, 100))
        return 3;

    for (uintptr_t i = 0; i < THREADS; ++i) {
        GlesRecordRtMetadata m =
            make_metadata(0x1000 + i * 0x100, i + 1);

        if (!gles_submission_snapshot_insert(&snapshot, &m))
            return 4;
    }

    pthread_t threads[THREADS];

    for (uintptr_t i = 0; i < THREADS; ++i) {
        if (pthread_create(&threads[i], NULL,
                           reader, (void *)i) != 0)
            return 5;
    }

    for (int i = 0; i < THREADS; ++i)
        pthread_join(threads[i], NULL);

    printf("R8C3 concurrent failures=%d\n",
           atomic_load(&failures));

    if (atomic_load(&failures) != 0)
        return 6;

    if (!gles_submission_snapshot_retire(&snapshot, 100))
        return 7;

    for (uint64_t i = 0; i < 500; ++i) {
        uint64_t sequence = 200 + i;

        if (!gles_submission_snapshot_begin(
                &snapshot, sequence))
            return 8;

        GlesRecordRtMetadata m =
            make_metadata(0x3000, i + 1);

        if (!gles_submission_snapshot_insert(
                &snapshot, &m))
            return 9;

        GlesRecordRtMetadata out = {0};

        if (!gles_submission_snapshot_lookup(
                &snapshot, m.record_key, &out))
            return 10;

        if (!gles_submission_snapshot_retire(
                &snapshot, sequence))
            return 11;

        if (gles_submission_snapshot_lookup(
                &snapshot, m.record_key, &out))
            return 12;
    }

    gles_submission_snapshot_destroy(&snapshot);

    puts("R8C3_RESULT PASS threads=4 reads=8000 reuse=500");
    return 0;
}