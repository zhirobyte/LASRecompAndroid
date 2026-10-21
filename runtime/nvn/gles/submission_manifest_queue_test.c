#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "submission_manifest_transport.h"
#include "submission_queue.h"

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
    GlesSubmissionEnvelope queued = {0};
    GlesRecordRtMetadata metadata = metadata_template();
    GlesRecordRtMetadata result = {0};

    GlesRecordCoordinatorKey first = {0};
    GlesRecordCoordinatorKey second = {0};
    GlesRecordCoordinatorKey replacement = {0};

    uint64_t sequence = 0;

    check("initialize bridge",
        gles_record_rt_bridge_init(&bridge));

    check("begin manifest",
        gles_submission_manifest_begin(&manifest));

    check("register first record",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x1000, 416},
            &metadata, &first));

    check("register second record",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x2000, 416},
            &metadata, &second));

    check("manifest add first",
        gles_submission_manifest_add(&manifest, first));

    check("manifest add second",
        gles_submission_manifest_add(&manifest, second));

    check("construct envelope from live records",
        gles_submission_manifest_to_envelope(
            &manifest, &bridge, 100, &envelope));

    check("constructed envelope validates",
        gles_submission_envelope_validate(&envelope));

    check("enqueue envelope",
        gles_submission_enqueue_with_envelope(
            0x12345000, &envelope, &sequence));

    check("queue assigned nonzero sequence",
        sequence != 0);

    check("retrieve queued envelope",
        gles_submission_get_envelope(
            sequence, &queued));

    check("queue replaced provisional sequence",
        queued.sequence == sequence);

    check("queued envelope validates",
        gles_submission_envelope_validate(&queued));

    check("queued envelope contains both records",
        queued.count == 2);

    check("first original generation survives queue",
        gles_submission_envelope_lookup(
            &queued, sequence, first, &result));

    check("second original generation survives queue",
        gles_submission_envelope_lookup(
            &queued, sequence, second, &result));

    /*
     * Do not execute GPU commands. The queue consumer merely
     * acknowledges a synthetic diagnostic submission.
     */
    GlesSubmission request = {0};

    check("dequeue synthetic request",
        gles_submission_dequeue(&request));

    check("dequeued handle preserved",
        request.command_handle == 0x12345000);

    check("dequeued sequence preserved",
        request.sequence == sequence);

    check("queued envelope remains retrievable",
        gles_submission_get_envelope(
            sequence, &queued));

    check("retire completed synthetic submission",
        (gles_submission_finish(
            sequence, false),
         gles_submission_retire(sequence)));

    check("retired envelope inaccessible",
        !gles_submission_get_envelope(
            sequence, &queued));

    /*
     * A stale manifest must not produce an envelope.
     */
    check("invalidate original first record",
        gles_record_rt_bridge_invalidate(
            &bridge,
            (GlesRecordRange){0x1000, 416}) >= 1);

    check("stale manifest transport rejected",
        !gles_submission_manifest_to_envelope(
            &manifest, &bridge, 101, &envelope));

    check("stale transport clears envelope",
        !envelope.active && envelope.count == 0);

    check("reject enqueue of cleared envelope",
        !gles_submission_enqueue_with_envelope(
            0x12345001, &envelope, &sequence));

    /*
     * A replacement at the same address must not make
     * the original generation valid again.
     */
    check("register replacement",
        gles_record_rt_bridge_register(
            &bridge,
            (GlesRecordRange){0x1000, 416},
            &metadata, &replacement));

    check("replacement has different generation",
        replacement.address == first.address &&
        replacement.generation != first.generation);

    check("original generation remains rejected",
        !gles_submission_manifest_to_envelope(
            &manifest, &bridge, 102, &envelope));

    gles_submission_manifest_clear(&manifest);

    check("begin replacement manifest",
        gles_submission_manifest_begin(&manifest));

    check("add replacement generation",
        gles_submission_manifest_add(
            &manifest, replacement));

    check("add surviving second generation",
        gles_submission_manifest_add(
            &manifest, second));

    check("construct replacement envelope",
        gles_submission_manifest_to_envelope(
            &manifest, &bridge, 103, &envelope));

    check("enqueue replacement envelope",
        gles_submission_enqueue_with_envelope(
            0x12345002, &envelope, &sequence));

    check("retrieve replacement envelope",
        gles_submission_get_envelope(
            sequence, &queued));

    check("replacement generation survives",
        gles_submission_envelope_lookup(
            &queued, sequence, replacement, &result));

    check("old generation absent",
        !gles_submission_envelope_lookup(
            &queued, sequence, first, &result));

    check("dequeue replacement request",
        gles_submission_dequeue(&request));

    check("finish replacement request",
        (gles_submission_finish(
            sequence, false),
         true));

    check("retire replacement request",
        gles_submission_retire(sequence));

    check("replacement envelope inaccessible after retire",
        !gles_submission_get_envelope(
            sequence, &queued));

    gles_submission_manifest_clear(&manifest);
    gles_record_rt_bridge_destroy(&bridge);

    printf("RESULT passed=%u failed=%u\n",
           passed, failed);

    return failed ? 1 : 0;
}