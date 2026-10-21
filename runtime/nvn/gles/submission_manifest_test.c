#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "submission_manifest.h"

static unsigned passed;
static unsigned failed;

static void check(const char *name, bool result)
{
    printf("%s %s\n", result ? "PASS" : "FAIL", name);

    if (result)
        ++passed;
    else
        ++failed;
}

typedef struct {
    uintptr_t address;
    uint64_t generation;
} LiveIdentity;

static bool is_live(
    GlesRecordCoordinatorKey key,
    void *context)
{
    const LiveIdentity *live = context;

    return key.address == live->address &&
           key.generation == live->generation;
}

int main(void)
{
    GlesSubmissionManifest manifest = {0};

    GlesRecordCoordinatorKey original = {
        .address = 0x1000,
        .generation = 7
    };

    GlesRecordCoordinatorKey reused = {
        .address = 0x1000,
        .generation = 8
    };

    GlesRecordCoordinatorKey second = {
        .address = 0x2000,
        .generation = 9
    };

    LiveIdentity live = {0x1000, 7};

    check("reject inactive add",
        !gles_submission_manifest_add(
            &manifest, original));

    check("begin manifest",
        gles_submission_manifest_begin(&manifest));

    check("reject second begin",
        !gles_submission_manifest_begin(&manifest));

    check("empty manifest valid",
        gles_submission_manifest_validate(&manifest));

    check("add original generation",
        gles_submission_manifest_add(
            &manifest, original));

    check("contains original generation",
        gles_submission_manifest_contains(
            &manifest, original));

    check("does not contain reused generation",
        !gles_submission_manifest_contains(
            &manifest, reused));

    check("reject duplicate identity",
        !gles_submission_manifest_add(
            &manifest, original));

    check("reject reused address",
        !gles_submission_manifest_add(
            &manifest, reused));

    check("reject zero address",
        !gles_submission_manifest_add(
            &manifest,
            (GlesRecordCoordinatorKey){0, 10}));

    check("reject zero generation",
        !gles_submission_manifest_add(
            &manifest,
            (GlesRecordCoordinatorKey){0x3000, 0}));

    check("original generation live",
        gles_submission_manifest_validate_live(
            &manifest, is_live, &live));

    live.generation = 8;

    check("stale generation rejected",
        !gles_submission_manifest_validate_live(
            &manifest, is_live, &live));

    live.generation = 7;

    check("add second identity",
        gles_submission_manifest_add(
            &manifest, second));

    check("two records structurally valid",
        gles_submission_manifest_validate(&manifest));

    check("second identity not yet live",
        !gles_submission_manifest_validate_live(
            &manifest, is_live, &live));

    check("reject null liveness callback",
        !gles_submission_manifest_validate_live(
            &manifest, NULL, &live));

    check("clear manifest",
        (gles_submission_manifest_clear(&manifest),
         !manifest.active && manifest.count == 0));

    check("restart manifest",
        gles_submission_manifest_begin(&manifest));

    bool capacity_ok = true;

    for (size_t i = 0;
         i < GLES_SUBMISSION_MANIFEST_CAPACITY;
         ++i) {
        GlesRecordCoordinatorKey key = {
            .address = (uintptr_t)(0x10000 + i * 16),
            .generation = (uint64_t)(i + 1)
        };

        if (!gles_submission_manifest_add(
                &manifest, key)) {
            capacity_ok = false;
            break;
        }
    }

    check("fill manifest capacity", capacity_ok);

    check("capacity count",
        manifest.count ==
            GLES_SUBMISSION_MANIFEST_CAPACITY);

    check("reject capacity overflow",
        !gles_submission_manifest_add(
            &manifest,
            (GlesRecordCoordinatorKey){
                .address = 0x999999,
                .generation = 999
            }));

    check("full manifest validates",
        gles_submission_manifest_validate(&manifest));

    manifest.keys[1].address = manifest.keys[0].address;

    check("detect corrupted duplicate address",
        !gles_submission_manifest_validate(&manifest));

    gles_submission_manifest_clear(&manifest);

    check("cleared manifest invalid",
        !gles_submission_manifest_validate(&manifest));

    printf("RESULT passed=%u failed=%u\n",
           passed, failed);

    return failed ? 1 : 0;
}