#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "record_rt_bridge.h"
#include "submission_snapshot.h"

static unsigned passed = 0;
static unsigned failed = 0;

#define CHECK(expr, label) do {                       \
    if (expr) {                                       \
        printf("PASS: %s\n", label);                   \
        ++passed;                                     \
    } else {                                          \
        printf("FAIL: %s\n", label);                   \
        ++failed;                                     \
    }                                                 \
} while (0)

static bool add_rt(
    GlesRecordRtBridge *bridge,
    uintptr_t address,
    GlesRecordCoordinatorKey *key)
{
    GlesRecordRtMetadata m = {0};
    m.valid = true;

    return gles_record_rt_bridge_register(
        bridge,
        (GlesRecordRange){address, 416},
        &m,
        key);
}

int main(void)
{
    GlesRecordRtBridge bridge = {0};
    GlesSubmissionSnapshot snapshot = {0};

    GlesRecordCoordinatorKey a = {0};
    GlesRecordCoordinatorKey b = {0};
    GlesRecordCoordinatorKey c = {0};
    GlesRecordCoordinatorKey replacement = {0};

    GlesRecordRtMetadata out = {0};
    size_t copied = 999;

    if (!gles_record_rt_bridge_init(&bridge)) {
        puts("FAIL: bridge initialization");
        return 2;
    }
    CHECK(true, "initialize bridge");

    if (!gles_submission_snapshot_init(&snapshot)) {
        puts("FAIL: snapshot initialization");
        gles_record_rt_bridge_destroy(&bridge);
        return 3;
    }
    CHECK(true, "initialize snapshot");

    CHECK(add_rt(&bridge, 0x10000, &a),
          "register RT A");
    CHECK(add_rt(&bridge, 0x20000, &b),
          "register RT B");
    CHECK(add_rt(&bridge, 0x30000, &c),
          "register RT C");

    CHECK(gles_submission_snapshot_begin(&snapshot, 100),
          "begin filtered snapshot");

    GlesRecordCoordinatorKey selected[2] = {b, a};

    CHECK(gles_record_rt_bridge_snapshot_copy_keys(
              &bridge, &snapshot, selected, 2, &copied),
          "copy only selected RT identities");

    CHECK(copied == 2, "exactly two entries copied");

    CHECK(gles_submission_snapshot_count(&snapshot) == 2,
          "filtered snapshot count");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, a, &out),
          "selected A present");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, b, &out),
          "selected B present");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, c, &out),
          "unselected C absent");

    CHECK(gles_submission_snapshot_retire(&snapshot, 100),
          "retire filtered snapshot");

    CHECK(gles_record_rt_bridge_invalidate(
              &bridge,
              (GlesRecordRange){0x10000, 416}) == 1,
          "invalidate A");

    CHECK(add_rt(&bridge, 0x10000, &replacement),
          "register replacement A");

    CHECK(replacement.generation != a.generation,
          "replacement has distinct generation");

    CHECK(gles_submission_snapshot_begin(&snapshot, 101),
          "begin stale-generation test");

    GlesRecordCoordinatorKey stale_keys[2] = {b, a};
    copied = 999;

    CHECK(!gles_record_rt_bridge_snapshot_copy_keys(
              &bridge, &snapshot,
              stale_keys, 2, &copied),
          "reject stale original generation");

    CHECK(copied == 1,
          "report bounded partial copy before stale key");

    CHECK(gles_submission_snapshot_retire(&snapshot, 101),
          "discard partial stale snapshot");

    CHECK(gles_submission_snapshot_count(&snapshot) == 0,
          "partial stale snapshot cleared");

    CHECK(gles_submission_snapshot_begin(&snapshot, 102),
          "begin replacement test");

    GlesRecordCoordinatorKey current_keys[2] = {
        replacement, c
    };
    copied = 999;

    CHECK(gles_record_rt_bridge_snapshot_copy_keys(
              &bridge, &snapshot,
              current_keys, 2, &copied),
          "copy current generations");

    CHECK(copied == 2, "two current generations copied");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, a, &out),
          "old A generation absent");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, replacement, &out),
          "replacement A generation present");

    CHECK(gles_submission_snapshot_retire(&snapshot, 102),
          "retire replacement test");

    CHECK(gles_submission_snapshot_begin(&snapshot, 103),
          "begin duplicate test");

    GlesRecordCoordinatorKey duplicate[2] = {b, b};
    copied = 999;

    CHECK(!gles_record_rt_bridge_snapshot_copy_keys(
              &bridge, &snapshot,
              duplicate, 2, &copied),
          "reject duplicate identity");

    CHECK(copied == 1,
          "duplicate rejection reports partial copy");

    CHECK(gles_submission_snapshot_retire(&snapshot, 103),
          "discard duplicate partial snapshot");

    CHECK(gles_submission_snapshot_begin(&snapshot, 104),
          "begin missing-key test");

    GlesRecordCoordinatorKey missing = {
        .address = 0xDEAD0000,
        .generation = 999
    };
    copied = 999;

    CHECK(!gles_record_rt_bridge_snapshot_copy_keys(
              &bridge, &snapshot,
              &missing, 1, &copied),
          "reject unknown record identity");

    CHECK(copied == 0,
          "unknown record copies nothing");

    CHECK(gles_submission_snapshot_retire(&snapshot, 104),
          "retire missing-key test");

    CHECK(gles_submission_snapshot_begin(&snapshot, 105),
          "begin empty-selection test");

    copied = 999;

    CHECK(gles_record_rt_bridge_snapshot_copy_keys(
              &bridge, &snapshot,
              NULL, 0, &copied),
          "accept empty key selection");

    CHECK(copied == 0, "empty selection copies zero");

    CHECK(gles_submission_snapshot_count(&snapshot) == 0,
          "empty snapshot remains empty");

    CHECK(gles_submission_snapshot_retire(&snapshot, 105),
          "retire empty-selection test");

    CHECK(gles_submission_snapshot_begin(&snapshot, 106),
          "begin capacity-boundary test");

    GlesRecordCoordinatorKey oversized[
        GLES_SUBMISSION_SNAPSHOT_CAPACITY + 1
    ] = {0};

    copied = 999;

    CHECK(!gles_record_rt_bridge_snapshot_copy_keys(
              &bridge,
              &snapshot,
              oversized,
              GLES_SUBMISSION_SNAPSHOT_CAPACITY + 1,
              &copied),
          "reject key count above capacity");

    CHECK(copied == 0,
          "oversized request copies nothing");

    CHECK(gles_submission_snapshot_count(&snapshot) == 0,
          "oversized request leaves snapshot empty");

    CHECK(gles_submission_snapshot_retire(&snapshot, 106),
          "retire capacity-boundary test");

    gles_submission_snapshot_destroy(&snapshot);
    gles_record_rt_bridge_destroy(&bridge);

    printf("R8D3B_RESULT passed=%u failed=%u\n",
           passed, failed);

    return failed == 0 ? 0 : 1;
}