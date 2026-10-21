#include "submission_manifest_transport.h"

#include <string.h>

bool gles_submission_manifest_to_envelope(
    const GlesSubmissionManifest *manifest,
    GlesRecordRtBridge *bridge,
    uint64_t provisional_sequence,
    GlesSubmissionEnvelope *out)
{
    if (!out)
        return false;

    memset(out, 0, sizeof(*out));

    if (!manifest ||
        !bridge ||
        !bridge->initialized ||
        !provisional_sequence ||
        !gles_submission_manifest_validate(manifest))
        return false;

    /*
     * Copy exact generations, never current-address identities.
     *
     * Hold the bridge's outer mutex across all lookups to avoid
     * accepting a mixture of registry generations.
     */
    if (pthread_mutex_lock(&bridge->mutex) != 0)
        return false;

    bool ok = gles_submission_envelope_begin(
        out, provisional_sequence);

    for (size_t i = 0; ok && i < manifest->count; ++i) {
        const GlesRecordCoordinatorKey key =
            manifest->keys[i];

        /*
         * The bridge's public lookup takes this same outer mutex.
         * While holding it, use the underlying registries directly
         * to avoid recursively locking a nonrecursive mutex.
         */
        GlesRecordRange range = {0};
        GlesRecordRtMetadata metadata = {0};

        ok = gles_record_coordinator_lookup(
            &bridge->records, key, &range);

        if (ok) {
            ok = gles_record_rt_sidecar_lookup(
                &bridge->sidecar, key, &metadata);
        }

        if (ok) {
            ok = gles_record_rt_metadata_matches(
                &metadata, key);
        }

        if (ok) {
            ok = gles_submission_envelope_insert(
                out, &metadata);
        }
    }

    if (ok)
        ok = gles_submission_envelope_validate(out);

    pthread_mutex_unlock(&bridge->mutex);

    if (!ok)
        memset(out, 0, sizeof(*out));

    return ok;
}