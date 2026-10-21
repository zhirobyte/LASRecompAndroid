#include <stdio.h>
#include <string.h>
#include "submission_snapshot.h"

static int passed, failed;

#define CHECK(condition, label) do { \
    if (condition) { \
        printf("PASS: %s\n", label); \
        passed++; \
    } else { \
        printf("FAIL: %s\n", label); \
        failed++; \
    } \
} while (0)

static GlesRecordRtMetadata make_metadata(
    uintptr_t address,
    uint64_t generation)
{
    GlesRecordRtMetadata m = {0};
    m.valid = true;
    m.record_key.address = address;
    m.record_key.generation = generation;
    return m;
}

int main(void)
{
    GlesSubmissionSnapshot snapshot = {0};
    GlesRecordRtMetadata out = {0};

    CHECK(gles_submission_snapshot_init(&snapshot),
          "initialize");

    CHECK(!gles_submission_snapshot_begin(&snapshot, 0),
          "reject zero sequence");

    CHECK(gles_submission_snapshot_begin(&snapshot, 100),
          "begin sequence 100");

    CHECK(!gles_submission_snapshot_begin(&snapshot, 101),
          "reject second active sequence");

    GlesRecordRtMetadata first =
        make_metadata(0x1000, 1);

    CHECK(gles_submission_snapshot_insert(&snapshot, &first),
          "insert first identity");

    CHECK(gles_submission_snapshot_count(&snapshot) == 1,
          "count first identity");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, first.record_key, &out),
          "lookup exact identity");

    CHECK(out.record_key.generation == 1,
          "preserve generation");

    CHECK(!gles_submission_snapshot_insert(&snapshot, &first),
          "reject duplicate identity");

    GlesRecordRtMetadata reused =
        make_metadata(0x1000, 2);

    CHECK(!gles_submission_snapshot_insert(&snapshot, &reused),
          "reject reused address with new generation");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, reused.record_key, &out),
          "reject mismatched generation");

    CHECK(!gles_submission_snapshot_retire(&snapshot, 101),
          "reject wrong retirement sequence");

    CHECK(gles_submission_snapshot_retire(&snapshot, 100),
          "retire sequence 100");

    CHECK(gles_submission_snapshot_count(&snapshot) == 0,
          "count zero after retirement");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, first.record_key, &out),
          "reject lookup after retirement");

    CHECK(gles_submission_snapshot_begin(&snapshot, 102),
          "begin new sequence");

    CHECK(gles_submission_snapshot_insert(&snapshot, &reused),
          "accept new generation in new sequence");

    CHECK(gles_submission_snapshot_lookup(
              &snapshot, reused.record_key, &out),
          "lookup new generation");

    CHECK(!gles_submission_snapshot_lookup(
              &snapshot, first.record_key, &out),
          "reject old generation");

    CHECK(gles_submission_snapshot_retire(&snapshot, 102),
          "retire sequence 102");

    CHECK(gles_submission_snapshot_begin(&snapshot, 103),
          "begin capacity test");

    for (size_t i = 0;
         i < GLES_SUBMISSION_SNAPSHOT_CAPACITY;
         ++i) {
        GlesRecordRtMetadata m =
            make_metadata(0x2000 + i * 0x100, i + 1);

        if (!gles_submission_snapshot_insert(&snapshot, &m)) {
            failed++;
            printf("FAIL: capacity insert %zu\n", i);
            break;
        }
    }

    CHECK(gles_submission_snapshot_count(&snapshot) ==
              GLES_SUBMISSION_SNAPSHOT_CAPACITY,
          "fill complete capacity");

    GlesRecordRtMetadata overflow =
        make_metadata(0x999999, 999);

    CHECK(!gles_submission_snapshot_insert(&snapshot, &overflow),
          "reject capacity overflow");

    CHECK(gles_submission_snapshot_retire(&snapshot, 103),
          "retire full snapshot");

    gles_submission_snapshot_destroy(&snapshot);

    printf("E167D_TEST_RESULT passed=%d failed=%d\n",
           passed, failed);

    return failed ? 1 : 0;
}