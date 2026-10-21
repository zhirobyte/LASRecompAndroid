#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "submission_envelope.h"

#define GLES_SUBMISSION_CAPACITY 64

typedef enum {
    GLES_SUBMISSION_PENDING = 0,
    GLES_SUBMISSION_COMPLETED = 1,
    GLES_SUBMISSION_FAILED = 2
} GlesSubmissionState;

typedef struct {
    uint64_t sequence;
    uint64_t command_handle;
} GlesSubmission;

bool gles_submission_enqueue(uint64_t handle, uint64_t *sequence);

/*
 * R8D-4E3A: diagnostic-only submission metadata.
 *
 * Metadata is copied under the queue mutex.
 * This does not pin guest memory or authorize GPU execution.
 */
bool gles_submission_enqueue_with_envelope(
    uint64_t handle,
    const GlesSubmissionEnvelope *input,
    uint64_t *sequence);

bool gles_submission_get_envelope(
    uint64_t sequence,
    GlesSubmissionEnvelope *out);
bool gles_submission_dequeue(GlesSubmission *out);

/*
 * Wait for an available submission for up to timeout_ms.
 * Returns false on timeout or invalid output.
 * Intended for future UI-thread consumption.
 */
bool gles_submission_wait_dequeue(GlesSubmission *out, unsigned timeout_ms);

/*
 * Wait for translation completion or failure.
 * This is NOT a GPU fence or GPU-completion guarantee.
 * The producer must retire the submission after this returns true.
 */
bool gles_submission_wait_completion(
    uint64_t sequence,
    unsigned timeout_ms,
    GlesSubmissionState *state);
bool gles_submission_finish(uint64_t sequence, bool success);
bool gles_submission_query(uint64_t sequence, GlesSubmissionState *state);
bool gles_submission_retire(uint64_t sequence);

// Reset is permitted only when producers and consumers are stopped.
// Returns false if requests are still pending.
bool gles_submission_reset(void);