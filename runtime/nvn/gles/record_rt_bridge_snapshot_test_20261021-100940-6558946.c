#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "record_rt_bridge.h"
#include "submission_snapshot.h"

static unsigned passed, failed;

#define CHECK(expr, label) do { \
    if (expr) { \
        printf("PASS: %s\n", label); \
        ++passed; \
    } else { \
        printf("FAIL: %s\n", label); \
        ++failed; \
    } \
} while (0)

static GlesRecordRtMetadata metadata(void)
{
    GlesRecordRtMetadata m = {0};
    m.valid = true;
    return m;
}

static bool add_rt(
    GlesRecordRtBridge *bridge,
    uintptr_t address,
    GlesRecordCoordinatorKey *out)
{
    GlesRecordRtMetadata m = metadata();
    return gles_record_rt_bridge_register(
        bridge,
        (GlesRecordRange){address, 416},
        &m,
        out);
}

int main(void)
{
    GlesRecordRtBridge bridge = {0};
    GlesSubmissionSnapshot snapshot = {0};
    GlesRecordCoordinatorKey a = {0};
    GlesRecordCoordinatorKey b = {0};
    GlesRecordRtMetadata out = {0};
    size_t copied = 999;

    CHECK(gles_record_rt_bridge_init(&bridge),
          "initialize bridge");

    CHECK(gles_submission_snapshot_init(&snapshot),
          "initialize snapshot");

    CHECK(add_rt(&bridge, 0x10000, &a),
          "register first RT");

    CHECK(add_rt(&bridge, 0x20000, &b),
          "register second RT");

    CHECK(gles_submission_snapshot_begin(&snapshot, 100),
          "begin snapshot");

    CHECK(gles_record_rt_bridge_snapshot_copy(
              &bridge, &snapshot, &copied),
          "copy RT metadata");

    CHECK(copied == 2, "copied two RT entries");

    CHECK(gles_submission_snapshot_count(&snapshot) == 2,
          "snapshot count two");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, a, &out),
          "lookup original first generation");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, b, &out),
          "lookup original second generation");

    CHECK(gles_record_rt_bridge_invalidate(
              &bridge, (GlesRecordRange){0x10000, 416}) == 1,
          "invalidate first RT");

    CHECK(!gles_record_rt_bridge_lookup(&bridge, a, &out),
          "old first RT no longer live in bridge");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, a, &out),
          "snapshot preserves copied first generation");

    GlesRecordCoordinatorKey replacement = {0};

    CHECK(add_rt(&bridge, 0x10000, &replacement),
          "reuse first RT address");

    CHECK(replacement.generation != a.generation,
          "reused RT receives new generation");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, replacement, &out),
          "snapshot rejects replacement generation");

    CHECK(gles_submission_snapshot_retire(&snapshot, 100),
          "retire original snapshot");

    CHECK(gles_submission_snapshot_begin(&snapshot, 101),
          "begin replacement snapshot");

    copied = 999;

    CHECK(gles_record_rt_bridge_snapshot_copy(
              &bridge, &snapshot, &copied),
          "copy current RT generations");

    CHECK(copied == 2,
          "copy two current RT entries");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, a, &out),
          "old generation absent in new snapshot");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, replacement, &out),
          "replacement generation present");

    CHECK(gles_submission_snapshot_retire(&snapshot, 101),
          "retire replacement snapshot");

    /*
     * Capacity failure: create more RT records than a snapshot
     * can hold, while staying below bridge capacity.
     */
    for (size_t i = 0;
         i < GLES_SUBMISSION_SNAPSHOT_CAPACITY;
         ++i) {
        GlesRecordCoordinatorKey key = {0};
        uintptr_t address = 0x100000 + i * 0x1000;

        if (!add_rt(&bridge, address, &key)) {
            printf("FAIL: capacity registration %zu\n", i);
            ++failed;
            break;
        }
    }

    CHECK(gles_submission_snapshot_begin(&snapshot, 102),
          "begin overflow snapshot");

    copied = 999;

    CHECK(!gles_record_rt_bridge_snapshot_copy(
              &bridge, &snapshot, &copied),
          "reject oversized snapshot");

    CHECK(copied <= GLES_SUBMISSION_SNAPSHOT_CAPACITY,
          "bounded partial copy");

    CHECK(gles_submission_snapshot_retire(&snapshot, 102),
          "discard failed partial snapshot");

    CHECK(gles_submission_snapshot_count(&snapshot) == 0,
          "failed snapshot fully retired");

    gles_submission_snapshot_destroy(&snapshot);
    gles_record_rt_bridge_destroy(&bridge);

    printf("R8D2_RESULT passed=%u failed=%u\n",
           passed, failed);

    return failed ? 1 : 0;
}