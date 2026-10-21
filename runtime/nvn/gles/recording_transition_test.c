#include "recording_transition.h"
#include <stdio.h>

static int checks, failures;

#define CHECK(x) do { \
    ++checks; \
    if (!(x)) { \
        ++failures; \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
    } \
} while (0)

static bool live(GlesRecordCoordinatorKey key, void *ctx)
{
    const GlesRecordCoordinatorKey *expected = ctx;

    return key.address == expected->address &&
           key.generation == expected->generation;
}

int main(void)
{
    GlesRecordingTransition state = {0};
    GlesSubmissionManifest manifest = {0};

    GlesTopologyListKey root = {0x1000, 1};
    GlesTopologyListKey chunk = {0x2000, 2};
    GlesRecordCoordinatorKey rt = {0x1010, 11};

    CHECK(gles_transition_begin(&state, root));
    CHECK(state.recording);
    CHECK(gles_transition_rt(&state, rt));

    CHECK(gles_transition_chunk(&state, chunk));
    CHECK(state.recording);
    CHECK(state.active.address == chunk.address);

    CHECK(!gles_transition_manifest(
        &state, root, live, &rt, &manifest));

    CHECK(gles_transition_end(&state));
    CHECK(!state.recording);

    CHECK(gles_transition_manifest(
        &state, root, live, &rt, &manifest));

    CHECK(manifest.count == 1);
    CHECK(gles_submission_manifest_contains(&manifest, rt));

    GlesRecordingTransition invalid = {0};
    CHECK(gles_transition_begin(&invalid, root));
    CHECK(!gles_transition_chunk(&invalid, root));
    CHECK(invalid.failed);
    CHECK(!gles_transition_end(&invalid));

    GlesRecordingTransition call = {0};
    GlesTopologyListKey child = {0x3000, 3};

    CHECK(gles_transition_begin(&call, child));
    CHECK(gles_transition_end(&call));

    CHECK(gles_transition_begin(&call, root));
    CHECK(gles_transition_call(&call, child));
    CHECK(gles_transition_end(&call));

    CHECK(gles_transition_manifest(
        &call, root, live, &rt, &manifest));

    CHECK(manifest.count == 0);

    GlesRecordingTransition missing = {0};
    CHECK(gles_transition_begin(&missing, root));
    CHECK(!gles_transition_call(&missing, child));
    CHECK(missing.failed);

    printf("R8D-4E4F-3D5C3A: %d/%d checks passed\n",
           checks - failures, checks);

    return failures ? 1 : 0;
}