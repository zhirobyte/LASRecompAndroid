#include "recording_event_adapter.h"
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
    GlesRecordingEventAdapter adapter = {0};
    GlesSubmissionManifest manifest = {0};

    GlesTopologyListKey root = {0x1000, 1};
    GlesTopologyListKey child = {0x2000, 2};
    GlesRecordCoordinatorKey rt = {0x1010, 11};

    CHECK(gles_recording_event_begin(&adapter, child));
    CHECK(gles_recording_event_end(&adapter));

    CHECK(gles_recording_event_begin(&adapter, root));
    CHECK(gles_recording_event_rt(&adapter, rt));
    CHECK(gles_recording_event_call(&adapter, child));
    CHECK(gles_recording_event_end(&adapter));

    CHECK(gles_recording_event_manifest(
        &adapter, root, live, &rt, &manifest));

    CHECK(manifest.count == 1);
    CHECK(gles_submission_manifest_contains(&manifest, rt));

    CHECK(!gles_recording_event_rt(&adapter, rt));
    CHECK(!gles_recording_event_end(&adapter));

    CHECK(!gles_recording_event_begin(&adapter, root));
    CHECK(adapter.failed);

    CHECK(!gles_recording_event_manifest(
        &adapter, root, live, &rt, &manifest));

    CHECK(!manifest.active && manifest.count == 0);

    GlesRecordingEventAdapter jump = {0};
    GlesTopologyListKey continuation = {0x4000, 4};
    GlesTopologyListKey start = {0x3000, 3};

    CHECK(gles_recording_event_begin(&jump, continuation));
    CHECK(gles_recording_event_end(&jump));

    CHECK(gles_recording_event_begin(&jump, start));
    CHECK(gles_recording_event_jump(&jump, continuation));

    CHECK(!jump.recording);
    CHECK(!gles_recording_event_rt(&jump, rt));

    CHECK(gles_recording_event_manifest(
        &jump, start, live, &rt, &manifest));

    CHECK(manifest.count == 0);

    GlesRecordingEventAdapter missing = {0};
    CHECK(gles_recording_event_begin(&missing, start));
    CHECK(!gles_recording_event_call(&missing, continuation));
    CHECK(missing.failed);

    printf("R8D-4E4F-3D5C1: %d/%d checks passed\n",
           checks - failures, checks);

    return failures ? 1 : 0;
}