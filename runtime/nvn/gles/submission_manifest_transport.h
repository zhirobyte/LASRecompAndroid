#ifndef GLES_SUBMISSION_MANIFEST_TRANSPORT_H
#define GLES_SUBMISSION_MANIFEST_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "submission_manifest.h"
#include "submission_envelope.h"
#include "record_rt_bridge.h"

/*
 * Diagnostic-only exact-key metadata transport.
 *
 * Source keys must have been captured at recording time.
 * No address-only generation reconstruction is permitted.
 *
 * This does not pin guest command memory or textures.
 */
bool gles_submission_manifest_to_envelope(
    const GlesSubmissionManifest *manifest,
    GlesRecordRtBridge *bridge,
    uint64_t provisional_sequence,
    GlesSubmissionEnvelope *out);

#endif