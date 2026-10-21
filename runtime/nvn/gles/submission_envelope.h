#ifndef GLES_SUBMISSION_ENVELOPE_H
#define GLES_SUBMISSION_ENVELOPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "record_rt_metadata.h"

#define GLES_SUBMISSION_ENVELOPE_CAPACITY 128

typedef struct {
    uint64_t sequence;
    size_t count;
    bool active;
    GlesRecordRtMetadata records[
        GLES_SUBMISSION_ENVELOPE_CAPACITY
    ];
} GlesSubmissionEnvelope;

/*
 * Validate the complete value-only envelope.
 * This does not validate command-memory or texture lifetime.
 */
bool gles_submission_envelope_validate(
    const GlesSubmissionEnvelope *envelope);
bool gles_submission_envelope_begin(
    GlesSubmissionEnvelope *envelope,
    uint64_t sequence);

bool gles_submission_envelope_insert(
    GlesSubmissionEnvelope *envelope,
    const GlesRecordRtMetadata *metadata);

bool gles_submission_envelope_lookup(
    const GlesSubmissionEnvelope *envelope,
    uint64_t sequence,
    GlesRecordCoordinatorKey key,
    GlesRecordRtMetadata *out);

bool gles_submission_envelope_retire(
    GlesSubmissionEnvelope *envelope,
    uint64_t sequence);

#endif