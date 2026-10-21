#ifndef GLES_SUBMISSION_MANIFEST_H
#define GLES_SUBMISSION_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "submission_envelope.h"

#define GLES_SUBMISSION_MANIFEST_CAPACITY \
    GLES_SUBMISSION_ENVELOPE_CAPACITY

typedef struct {
    GlesRecordCoordinatorKey keys[
        GLES_SUBMISSION_MANIFEST_CAPACITY
    ];
    size_t count;
    bool active;
} GlesSubmissionManifest;

/*
 * Value-only diagnostic manifest.
 *
 * Keys must originate from recording-time registration.
 * Never reconstruct generations from addresses.
 *
 * No command-memory or texture lifetime pinning.
 */
bool gles_submission_manifest_begin(
    GlesSubmissionManifest *manifest);

bool gles_submission_manifest_add(
    GlesSubmissionManifest *manifest,
    GlesRecordCoordinatorKey key);

bool gles_submission_manifest_contains(
    const GlesSubmissionManifest *manifest,
    GlesRecordCoordinatorKey key);

bool gles_submission_manifest_validate(
    const GlesSubmissionManifest *manifest);

bool gles_submission_manifest_validate_live(
    const GlesSubmissionManifest *manifest,
    bool (*is_live)(
        GlesRecordCoordinatorKey key,
        void *context),
    void *context);

void gles_submission_manifest_clear(
    GlesSubmissionManifest *manifest);

#endif