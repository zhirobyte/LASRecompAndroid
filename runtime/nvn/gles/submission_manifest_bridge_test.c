#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "submission_manifest.h"
#include "record_rt_bridge.h"

static unsigned passed;
static unsigned failed;

static void check(const char *name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);

    if (ok)
        ++passed;
    else
        ++failed;
}

static bool bridge_key_live(
    GlesRecordCoordinatorKey key,
    void *context)
{
    GlesRecordRtBridge *bridge = context;
    GlesRecordRtMetadata metadata = {0};

    return gles_record_rt_bridge_lookup(
        bridge, key, &metadata);
}

static GlesRecordRtMetadata empty_rt_metadata(void)
{
    GlesRecordRtMetadata metadata = {0};

    metadata.valid = true;
    metadata.color_count = 0;

    return metadata;
}

int main(void)
{
    GlesRecordRtBridge bridge = {0};
    GlesSubmissionManifest manifest = {0};

    GlesRecordCoordinatorKey first = {0};
    GlesRecordCoordinatorKey replacement = {0};
    GlesRecordCoordinatorKey second = {0};

    GlesRecordRtMetadata metadata = empty_rt_metadata();

    check("initialize bridge",
        gles_record_rt_bridge_init(&bridge));

    check("begin manifest",
        gles_submission_manifest_begin(&manifest));

    check("register first record",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x1000, 416},
            &metadata,
            &first));

    check("first key has identity",
        first.address == 0x1000 &&
        first.generation != 0);

    check("add original key",
        gles_submission_manifest_add(
            &manifest, first));

    check("original key is live",
        gles_submission_manifest_validate_live(
            &manifest, bridge_key_live, &bridge));

    check("invalidate original range",
        gles_record_rt_bridge_invalidate(
            &bridge,
            (GlesRecordRange){0x1000, 416}) >= 1);

    check("original key becomes stale",
        !gles_submission_manifest_validate_live(
            &manifest, bridge_key_live, &bridge));

    check("register replacement at same address",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x1000, 416},
            &metadata,
            &replacement));

    check("replacement has new generation",
        replacement.address == first.address &&
        replacement.generation != first.generation);

    check("old manifest remains stale",
        !gles_submission_manifest_validate_live(
            &manifest, bridge_key_live, &bridge));

    check("clear original manifest",
        (gles_submission_manifest_clear(&manifest),
         !manifest.active));

    check("begin replacement manifest",
        gles_submission_manifest_begin(&manifest));

    check("add replacement key",
        gles_submission_manifest_add(
            &manifest, replacement));

    check("replacement manifest is live",
        gles_submission_manifest_validate_live(
            &manifest, bridge_key_live, &bridge));

    check("register second independent record",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x2000, 416},
            &metadata,
            &second));

    check("add second key",
        gles_submission_manifest_add(
            &manifest, second));

    check("both records live",
        gles_submission_manifest_validate_live(
            &manifest, bridge_key_live, &bridge));

    check("invalidate second record only",
        gles_record_rt_bridge_invalidate(
            &bridge,
            (GlesRecordRange){0x2000, 416}) >= 1);

    check("multi-record manifest rejects stale member",
        !gles_submission_manifest_validate_live(
            &manifest, bridge_key_live, &bridge));

    check("replacement record still live",
        gles_record_rt_bridge_lookup(
            &bridge, replacement, &metadata));

    gles_submission_manifest_clear(&manifest);
    gles_record_rt_bridge_destroy(&bridge);

    printf("RESULT passed=%u failed=%u\n",
           passed, failed);

    return failed ? 1 : 0;
}