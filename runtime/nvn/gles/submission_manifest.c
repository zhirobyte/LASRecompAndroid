#include "submission_manifest.h"

#include <string.h>

bool gles_submission_manifest_begin(
    GlesSubmissionManifest *manifest)
{
    if (!manifest || manifest->active)
        return false;

    memset(manifest, 0, sizeof(*manifest));
    manifest->active = true;
    return true;
}

bool gles_submission_manifest_contains(
    const GlesSubmissionManifest *manifest,
    GlesRecordCoordinatorKey key)
{
    if (!manifest || !manifest->active ||
        !key.address || !key.generation ||
        manifest->count > GLES_SUBMISSION_MANIFEST_CAPACITY)
        return false;

    for (size_t i = 0; i < manifest->count; ++i) {
        if (manifest->keys[i].address == key.address &&
            manifest->keys[i].generation == key.generation)
            return true;
    }

    return false;
}

bool gles_submission_manifest_add(
    GlesSubmissionManifest *manifest,
    GlesRecordCoordinatorKey key)
{
    if (!manifest || !manifest->active ||
        !key.address || !key.generation ||
        manifest->count >= GLES_SUBMISSION_MANIFEST_CAPACITY)
        return false;

    /*
     * Reject duplicate addresses even when generations differ.
     * A single manifest cannot contain two identities for the
     * same command-record location.
     */
    for (size_t i = 0; i < manifest->count; ++i) {
        if (manifest->keys[i].address == key.address)
            return false;
    }

    manifest->keys[manifest->count++] = key;
    return true;
}

bool gles_submission_manifest_validate(
    const GlesSubmissionManifest *manifest)
{
    if (!manifest || !manifest->active ||
        manifest->count > GLES_SUBMISSION_MANIFEST_CAPACITY)
        return false;

    for (size_t i = 0; i < manifest->count; ++i) {
        GlesRecordCoordinatorKey key = manifest->keys[i];

        if (!key.address || !key.generation)
            return false;

        for (size_t j = 0; j < i; ++j) {
            if (manifest->keys[j].address == key.address)
                return false;
        }
    }

    return true;
}

bool gles_submission_manifest_validate_live(
    const GlesSubmissionManifest *manifest,
    bool (*is_live)(
        GlesRecordCoordinatorKey key,
        void *context),
    void *context)
{
    if (!is_live ||
        !gles_submission_manifest_validate(manifest))
        return false;

    for (size_t i = 0; i < manifest->count; ++i) {
        if (!is_live(manifest->keys[i], context))
            return false;
    }

    return true;
}

void gles_submission_manifest_clear(
    GlesSubmissionManifest *manifest)
{
    if (manifest)
        memset(manifest, 0, sizeof(*manifest));
}