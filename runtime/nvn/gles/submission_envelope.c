#include "submission_envelope.h"

#include <string.h>

bool gles_submission_envelope_validate(
    const GlesSubmissionEnvelope *envelope)
{
    if (!envelope ||
        !envelope->active ||
        !envelope->sequence ||
        envelope->count > GLES_SUBMISSION_ENVELOPE_CAPACITY)
        return false;

    for (size_t i = 0; i < envelope->count; ++i) {
        const GlesRecordRtMetadata *entry =
            &envelope->records[i];

        if (!gles_record_rt_metadata_validate(entry))
            return false;

        if (!entry->record_key.address ||
            !entry->record_key.generation)
            return false;

        for (size_t j = 0; j < i; ++j) {
            if (envelope->records[j].record_key.address ==
                entry->record_key.address)
                return false;
        }
    }

    return true;
}
bool gles_submission_envelope_begin(
    GlesSubmissionEnvelope *envelope,
    uint64_t sequence)
{
    if (!envelope || !sequence || envelope->active)
        return false;

    memset(envelope, 0, sizeof(*envelope));
    envelope->sequence = sequence;
    envelope->active = true;
    return true;
}

bool gles_submission_envelope_insert(
    GlesSubmissionEnvelope *envelope,
    const GlesRecordRtMetadata *metadata)
{
    if (!envelope || !envelope->active ||
        !metadata ||
        !gles_record_rt_metadata_validate(metadata) ||
        envelope->count >= GLES_SUBMISSION_ENVELOPE_CAPACITY)
        return false;

    for (size_t i = 0; i < envelope->count; ++i) {
        if (envelope->records[i].record_key.address ==
            metadata->record_key.address)
            return false;
    }

    envelope->records[envelope->count++] = *metadata;
    return true;
}

bool gles_submission_envelope_lookup(
    const GlesSubmissionEnvelope *envelope,
    uint64_t sequence,
    GlesRecordCoordinatorKey key,
    GlesRecordRtMetadata *out)
{
    if (!envelope || !envelope->active ||
        envelope->sequence != sequence || !out ||
        !key.address || !key.generation)
        return false;

    for (size_t i = 0; i < envelope->count; ++i) {
        const GlesRecordRtMetadata *entry =
            &envelope->records[i];

        if (gles_record_rt_metadata_matches(entry, key)) {
            *out = *entry;
            return true;
        }
    }

    return false;
}

bool gles_submission_envelope_retire(
    GlesSubmissionEnvelope *envelope,
    uint64_t sequence)
{
    if (!envelope || !envelope->active ||
        envelope->sequence != sequence)
        return false;

    memset(envelope, 0, sizeof(*envelope));
    return true;
}