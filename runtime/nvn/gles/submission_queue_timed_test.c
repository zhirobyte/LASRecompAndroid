#include "submission_queue.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PRODUCERS 4
#define PER_PRODUCER 2500
#define TOTAL (PRODUCERS * PER_PRODUCER)

static atomic_uint consumed;
static atomic_uint failures;
static atomic_uchar seen[TOTAL + 1];

static void *producer(void *arg)
{
    uintptr_t id = (uintptr_t)arg;

    for (unsigned i = 0; i < PER_PRODUCER; i++) {
        uint64_t handle = id * PER_PRODUCER + i + 1;
        uint64_t sequence = 0;

        while (!gles_submission_enqueue(handle, &sequence))
            sched_yield();
    }

    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;

    while (atomic_load(&consumed) < TOTAL) {
        GlesSubmission submission;

        if (!gles_submission_wait_dequeue(&submission, 50))
            continue;

        uint64_t handle = submission.command_handle;

        if (!handle || handle > TOTAL) {
            atomic_fetch_add(&failures, 1);
        } else if (atomic_exchange(&seen[handle], 1)) {
            atomic_fetch_add(&failures, 1);
        }

        if (!gles_submission_finish(submission.sequence, true))
            atomic_fetch_add(&failures, 1);

        if (!gles_submission_retire(submission.sequence))
            atomic_fetch_add(&failures, 1);

        atomic_fetch_add(&consumed, 1);
    }

    return NULL;
}

int main(void)
{
    pthread_t producers[PRODUCERS];
    pthread_t consumer_thread;

    if (pthread_create(&consumer_thread, NULL, consumer, NULL))
        return 2;

    for (uintptr_t i = 0; i < PRODUCERS; i++) {
        if (pthread_create(&producers[i], NULL, producer,
                           (void *)i))
            return 3;
    }

    for (unsigned i = 0; i < PRODUCERS; i++)
        pthread_join(producers[i], NULL);

    pthread_join(consumer_thread, NULL);

    for (unsigned i = 1; i <= TOTAL; i++) {
        if (!atomic_load(&seen[i]))
            atomic_fetch_add(&failures, 1);
    }

    printf("E111 consumed=%u expected=%u failures=%u\n",
           atomic_load(&consumed),
           TOTAL,
           atomic_load(&failures));

    return atomic_load(&failures) ? 1 : 0;
}