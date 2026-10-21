#include "submission_queue.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define PRODUCERS 4
#define PER_PRODUCER 2500
#define TOTAL (PRODUCERS * PER_PRODUCER)

static atomic_uint produced;
static atomic_uint consumed;
static atomic_uint retired;
static atomic_bool failed;

static void *producer_main(void *argument) {
    uintptr_t producer = (uintptr_t)argument;

    for (unsigned i = 0; i < PER_PRODUCER; i++) {
        uint64_t handle = 1 + producer * PER_PRODUCER + i;
        uint64_t sequence = 0;

        while (!gles_submission_enqueue(handle, &sequence)) {
            if (atomic_load(&failed)) return NULL;
            sched_yield();
        }

        if (!sequence) {
            atomic_store(&failed, true);
            return NULL;
        }

        atomic_fetch_add(&produced, 1);
    }

    return NULL;
}

static void *consumer_main(void *unused) {
    (void)unused;

    unsigned completed = 0;
    uint64_t previous = 0;

    while (completed < TOTAL) {
        GlesSubmission request = {0};

        if (!gles_submission_dequeue(&request)) {
            if (atomic_load(&failed)) return NULL;
            sched_yield();
            continue;
        }

        if (!request.command_handle ||
            request.command_handle > TOTAL ||
            request.sequence <= previous) {
            atomic_store(&failed, true);
            return NULL;
        }

        previous = request.sequence;

        if (!gles_submission_finish(request.sequence, true)) {
            atomic_store(&failed, true);
            return NULL;
        }

        GlesSubmissionState state;

        if (!gles_submission_query(request.sequence, &state) ||
            state != GLES_SUBMISSION_COMPLETED) {
            atomic_store(&failed, true);
            return NULL;
        }

        if (!gles_submission_retire(request.sequence)) {
            atomic_store(&failed, true);
            return NULL;
        }

        atomic_fetch_add(&consumed, 1);
        atomic_fetch_add(&retired, 1);
        completed++;
    }

    return NULL;
}

int main(void) {
    pthread_t producers[PRODUCERS];
    pthread_t consumer;

    assert(gles_submission_reset());

    assert(pthread_create(&consumer, NULL, consumer_main, NULL) == 0);

    for (uintptr_t i = 0; i < PRODUCERS; i++) {
        assert(pthread_create(
            &producers[i],
            NULL,
            producer_main,
            (void *)i
        ) == 0);
    }

    for (unsigned i = 0; i < PRODUCERS; i++) {
        assert(pthread_join(producers[i], NULL) == 0);
    }

    assert(pthread_join(consumer, NULL) == 0);

    assert(!atomic_load(&failed));
    assert(atomic_load(&produced) == TOTAL);
    assert(atomic_load(&consumed) == TOTAL);
    assert(atomic_load(&retired) == TOTAL);

    assert(gles_submission_reset());

    printf(
        "E93 PASS: produced=%u consumed=%u retired=%u\n",
        atomic_load(&produced),
        atomic_load(&consumed),
        atomic_load(&retired)
    );

    return 0;
}