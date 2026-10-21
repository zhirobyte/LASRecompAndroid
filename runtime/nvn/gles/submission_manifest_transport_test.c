#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "submission_manifest_transport.h"

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

static GlesRecordRtMetadata metadata_template(void)
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
    GlesSubmissionEnvelope envelope = {0};

    GlesRecordCoordinatorKey first = {0};
    GlesRecordCoordinatorKey second = {0};
    GlesRecordCoordinatorKey replacement = {0};

    GlesRecordRtMetadata metadata = metadata_template();
    GlesRecordRtMetadata result = {0};

    check("initialize bridge",
        gles_record_rt_bridge_init(&bridge));

    check("begin manifest",
        gles_submission_manifest_begin(&manifest));

    check("register first",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x1000, 416},
            &metadata, &first));

    check("register second",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x2000, 416},
            &metadata, &second));

    check("add first original key",
        gles_submission_manifest_add(&manifest, first));

    check("add second original key",
        gles_submission_manifest_add(&manifest, second));

    check("transport two live records",
        gles_submission_manifest_to_envelope(
            &manifest, &bridge, 100, &envelope));

    check("envelope sequence",
        envelope.sequence == 100);

    check("envelope contains two records",
        envelope.count == 2);

    check("first generation preserved",
        gles_submission_envelope_lookup(
            &envelope, 100, first, &result));

    check("second generation preserved",
        gles_submission_envelope_lookup(
            &envelope, 100, second, &result));

    check("envelope validates",
        gles_submission_envelope_validate(&envelope));

    check("invalidate first",
        gles_record_rt_bridge_invalidate(
            &bridge,
            (GlesRecordRange){0x1000, 416}) >= 1);

    check("stale transport rejected",
        !gles_submission_manifest_to_envelope(
            &manifest, &bridge, 101, &envelope));

    check("failed transport clears envelope",
        !envelope.active && envelope.count == 0);

    check("register replacement",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x1000, 416},
            &metadata, &replacement));

    check("replacement generation differs",
        replacement.address == first.address &&
        replacement.generation != first.generation);

    check("old key still rejected",
        !gles_submission_manifest_to_envelope(
            &manifest, &bridge, 102, &envelope));

    gles_submission_manifest_clear(&manifest);

    check("restart manifest",
        gles_submission_manifest_begin(&manifest));

    check("add replacement",
        gles_submission_manifest_add(
            &manifest, replacement));

    check("add surviving second",
        gles_submission_manifest_add(
            &manifest, second));

    check("replacement transport succeeds",
        gles_submission_manifest_to_envelope(
            &manifest, &bridge, 103, &envelope));

    check("replacement identity present",
        gles_submission_envelope_lookup(
            &envelope, 103, replacement, &result));

    check("old generation absent",
        !gles_submission_envelope_lookup(
            &envelope, 103, first, &result));

    check("reject zero sequence",
        !gles_submission_manifest_to_envelope(
            &manifest, &bridge, 0, &envelope));

    check("zero sequence clears output",
        !envelope.active && envelope.count == 0);

    gles_submission_manifest_clear(&manifest);
    gles_record_rt_bridge_destroy(&bridge);

    printf("RESULT passed=%u failed=%u\n",
           passed, failed);

    return failed ? 1 : 0;
}