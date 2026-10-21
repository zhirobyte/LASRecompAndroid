#include <time.h>
#include "submission_queue.h"
#include "submission_envelope.h"

#include <pthread.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    GlesSubmission request;
    GlesSubmissionState state;
    GlesSubmissionEnvelope envelope;
    bool occupied;
    bool dequeued;
    bool has_envelope;
} SubmissionSlot;

static SubmissionSlot slots[GLES_SUBMISSION_CAPACITY];
static pthread_mutex_t submission_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t submission_changed = PTHREAD_COND_INITIALIZER;
static uint64_t next_sequence = 1;

bool gles_submission_enqueue_with_envelope(
    uint64_t handle,
    const GlesSubmissionEnvelope *input,
    uint64_t *sequence)
{
    if (!handle || !sequence)
        return false;

    /*
     * R8D-4E3C:
     * Reject malformed metadata before publication.
     * The caller's sequence is a provisional envelope identity;
     * the queue assigns the authoritative submission sequence.
     */
    if (input && !gles_submission_envelope_validate(input))
        return false;

    pthread_mutex_lock(&submission_lock);

    SubmissionSlot *free_slot = NULL;

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        if (!slots[i].occupied) {
            free_slot = &slots[i];
            break;
        }
    }

    if (!free_slot || next_sequence == 0) {
        pthread_mutex_unlock(&submission_lock);
        return false;
    }

    uint64_t id = next_sequence++;

    /*
     * R8D-4E3A:
     * Publish the submission and its value-only metadata together,
     * while holding submission_lock.
     *
     * The caller's envelope is never retained by pointer.
     */
    SubmissionSlot prepared = {0};

    prepared.request.sequence = id;
    prepared.request.command_handle = handle;
    prepared.state = GLES_SUBMISSION_PENDING;
    prepared.occupied = true;

    if (input) {
        prepared.envelope = *input;
        prepared.envelope.sequence = id;
        prepared.has_envelope = true;
    }

    *free_slot = prepared;

    *sequence = id;

    pthread_cond_broadcast(&submission_changed);
    pthread_mutex_unlock(&submission_lock);
    return true;
}


bool gles_submission_enqueue(
    uint64_t handle,
    uint64_t *sequence)
{
    return gles_submission_enqueue_with_envelope(
        handle, NULL, sequence);
}

bool gles_submission_get_envelope(
    uint64_t sequence,
    GlesSubmissionEnvelope *out)
{
    if (!sequence || !out)
        return false;

    pthread_mutex_lock(&submission_lock);

    bool found = false;

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; ++i) {
        SubmissionSlot *slot = &slots[i];

        if (!slot->occupied ||
            slot->request.sequence != sequence ||
            !slot->has_envelope)
            continue;

        *out = slot->envelope;
        found = true;
        break;
    }

    pthread_mutex_unlock(&submission_lock);
    return found;
}
bool gles_submission_dequeue(GlesSubmission *out) {
    if (!out) return false;

    pthread_mutex_lock(&submission_lock);

    SubmissionSlot *candidate = NULL;

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        SubmissionSlot *slot = &slots[i];

        if (!slot->occupied || slot->dequeued ||
            slot->state != GLES_SUBMISSION_PENDING)
            continue;

        if (!candidate ||
            slot->request.sequence < candidate->request.sequence)
            candidate = slot;
    }

    if (!candidate) {
        pthread_mutex_unlock(&submission_lock);
        return false;
    }

    candidate->dequeued = true;
    *out = candidate->request;

    pthread_mutex_unlock(&submission_lock);
    return true;
}

bool gles_submission_finish(uint64_t sequence, bool success) {
    pthread_mutex_lock(&submission_lock);

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        SubmissionSlot *slot = &slots[i];

        if (slot->occupied &&
            slot->request.sequence == sequence &&
            slot->dequeued &&
            slot->state == GLES_SUBMISSION_PENDING) {
            slot->state = success
                ? GLES_SUBMISSION_COMPLETED
                : GLES_SUBMISSION_FAILED;

            pthread_cond_broadcast(&submission_changed);
            pthread_mutex_unlock(&submission_lock);
            return true;
        }
    }

    pthread_mutex_unlock(&submission_lock);
    return false;
}

bool gles_submission_query(uint64_t sequence, GlesSubmissionState *state) {
    if (!state || !sequence) return false;

    pthread_mutex_lock(&submission_lock);

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        if (slots[i].occupied &&
            slots[i].request.sequence == sequence) {
            *state = slots[i].state;
            pthread_mutex_unlock(&submission_lock);
            return true;
        }
    }

    pthread_mutex_unlock(&submission_lock);
    return false;
}

bool gles_submission_retire(uint64_t sequence) {
    if (!sequence) return false;

    pthread_mutex_lock(&submission_lock);

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        SubmissionSlot *slot = &slots[i];

        if (!slot->occupied ||
            slot->request.sequence != sequence)
            continue;

        if (slot->state == GLES_SUBMISSION_PENDING) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }

        memset(slot, 0, sizeof *slot);

        pthread_cond_broadcast(&submission_changed);
        pthread_mutex_unlock(&submission_lock);
        return true;
    }

    pthread_mutex_unlock(&submission_lock);
    return false;
}
bool gles_submission_reset(void) {
    pthread_mutex_lock(&submission_lock);

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        if (slots[i].occupied &&
            slots[i].state == GLES_SUBMISSION_PENDING) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }
    }

    memset(slots, 0, sizeof slots);

    // Preserve monotonically increasing sequence numbers.
    pthread_mutex_unlock(&submission_lock);
    return true;
}

/*
 * Timed consumer.
 * The availability predicate and condition wait share submission_lock.
 * This avoids lost notifications and tolerates spurious wakeups.
 */
bool gles_submission_wait_dequeue(GlesSubmission *out, unsigned timeout_ms)
{
    if (!out)
        return false;

    struct timespec deadline = {0};

    if (timeout_ms) {
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0)
            return false;

        deadline.tv_sec += timeout_ms / 1000u;
        deadline.tv_nsec += (long)(timeout_ms % 1000u) * 1000000L;

        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    pthread_mutex_lock(&submission_lock);

    for (;;) {
        SubmissionSlot *candidate = NULL;

        for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
            SubmissionSlot *slot = &slots[i];

            if (!slot->occupied ||
                slot->dequeued ||
                slot->state != GLES_SUBMISSION_PENDING)
                continue;

            if (!candidate ||
                slot->request.sequence < candidate->request.sequence)
                candidate = slot;
        }

        if (candidate) {
            *out = candidate->request;
            candidate->dequeued = true;
            pthread_mutex_unlock(&submission_lock);
            return true;
        }

        if (!timeout_ms) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }

        int result = pthread_cond_timedwait(
            &submission_changed,
            &submission_lock,
            &deadline
        );

        if (result != 0) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }
    }
}

/*
 * Wait for a particular submission to reach a terminal state.
 * The condition predicate and wait share submission_lock.
 *
 * COMPLETED means the consumer reported success; it does not
 * automatically imply that GL commands finished on the GPU.
 */
bool gles_submission_wait_completion(
    uint64_t sequence,
    unsigned timeout_ms,
    GlesSubmissionState *state)
{
    if (!sequence || !state)
        return false;

    struct timespec deadline = {0};

    if (timeout_ms) {
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0)
            return false;

        deadline.tv_sec += timeout_ms / 1000u;
        deadline.tv_nsec += (long)(timeout_ms % 1000u) * 1000000L;

        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    pthread_mutex_lock(&submission_lock);

    for (;;) {
        SubmissionSlot *found = NULL;

        for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
            SubmissionSlot *slot = &slots[i];

            if (slot->occupied &&
                slot->request.sequence == sequence) {
                found = slot;
                break;
            }
        }

        if (!found) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }

        if (found->state != GLES_SUBMISSION_PENDING) {
            *state = found->state;
            pthread_mutex_unlock(&submission_lock);
            return true;
        }

        if (!timeout_ms) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }

        int result = pthread_cond_timedwait(
            &submission_changed,
            &submission_lock,
            &deadline
        );

        if (result != 0) {
            pthread_mutex_unlock(&submission_lock);
            return false;
        }
    }
}