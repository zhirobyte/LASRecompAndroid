#include "submission_queue.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>

#define ITERATIONS 1000

static atomic_uint failures;

static void *consumer(void *unused)
{
    (void)unused;

    for (unsigned i = 0; i < ITERATIONS; ++i) {
        GlesSubmission job;

        if (!gles_submission_wait_dequeue(&job, 5000)) {
            atomic_fetch_add(&failures, 1);
            return NULL;
        }

        if (job.command_handle != (uint64_t)i + 1)
            atomic_fetch_add(&failures, 1);

        if (!gles_submission_finish(job.sequence, true))
            atomic_fetch_add(&failures, 1);
    }

    return NULL;
}

int main(void)
{
    GlesSubmission job;
    GlesSubmissionState state;
    uint64_t sequence;

    if (gles_submission_wait_dequeue(&job, 0))
        return 1;

    if (gles_submission_wait_completion(999999, 0, &state))
        return 2;

    if (!gles_submission_enqueue(123456, &sequence))
        return 3;

    if (!gles_submission_wait_dequeue(&job, 0))
        return 4;

    if (gles_submission_wait_completion(sequence, 0, &state))
        return 5;

    if (!gles_submission_finish(sequence, false))
        return 6;

    if (!gles_submission_wait_completion(sequence, 0, &state))
        return 7;

    if (state != GLES_SUBMISSION_FAILED)
        return 8;

    if (!gles_submission_retire(sequence))
        return 9;

    pthread_t worker;

    if (pthread_create(&worker, NULL, consumer, NULL))
        return 10;

    for (unsigned i = 0; i < ITERATIONS; ++i) {
        uint64_t seq = 0;

        if (!gles_submission_enqueue((uint64_t)i + 1, &seq))
            return 11;

        if (!gles_submission_wait_completion(seq, 5000, &state))
            return 12;

        if (state != GLES_SUBMISSION_COMPLETED)
            return 13;

        if (!gles_submission_retire(seq))
            return 14;
    }

    pthread_join(worker, NULL);

    if (atomic_load(&failures))
        return 15;

    if (!gles_submission_reset())
        return 16;

    printf("E114 PASS: %d handshakes, failures=0\n", ITERATIONS);
    return 0;
}