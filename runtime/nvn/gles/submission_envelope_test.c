#include "submission_envelope.h"

#include <stdio.h>
#include <string.h>

static unsigned passed;
static unsigned failed;

static void check(const char *name, bool condition)
{
    if (condition) {
        ++passed;
        printf("PASS %s\n", name);
    } else {
        ++failed;
        printf("FAIL %s\n", name);
    }
}

static GlesRecordRtMetadata make_record(
    uintptr_t address,
    uint64_t generation)
{
    GlesRecordRtMetadata metadata = {0};
    metadata.valid = true;
    metadata.record_key.address = address;
    metadata.record_key.generation = generation;
    return metadata;
}

int main(void)
{
    GlesSubmissionEnvelope envelope = {0};

    GlesRecordRtMetadata a = make_record(0x1000, 1);
    GlesRecordRtMetadata b = make_record(0x2000, 2);
    GlesRecordRtMetadata reused = make_record(0x1000, 3);
    GlesRecordRtMetadata output = {0};

    check("begin", gles_submission_envelope_begin(
        &envelope, 42));

    check("reject second begin",
        !gles_submission_envelope_begin(&envelope, 43));

    check("insert A",
        gles_submission_envelope_insert(&envelope, &a));

    check("insert B",
        gles_submission_envelope_insert(&envelope, &b));

    check("reject duplicate",
        !gles_submission_envelope_insert(&envelope, &a));

    check("reject reused address",
        !gles_submission_envelope_insert(&envelope, &reused));

    check("lookup original",
        gles_submission_envelope_lookup(
            &envelope, 42, a.record_key, &output));

    check("original generation",
        output.record_key.generation == 1);

    check("reject wrong sequence",
        !gles_submission_envelope_lookup(
            &envelope, 43, a.record_key, &output));

    check("reject reused generation",
        !gles_submission_envelope_lookup(
            &envelope, 42, reused.record_key, &output));

    check("reject wrong retirement",
        !gles_submission_envelope_retire(&envelope, 43));

    check("retire",
        gles_submission_envelope_retire(&envelope, 42));

    check("reject lookup after retirement",
        !gles_submission_envelope_lookup(
            &envelope, 42, a.record_key, &output));

    check("begin next sequence",
        gles_submission_envelope_begin(&envelope, 44));

    check("reject zero sequence",
        !gles_submission_envelope_begin(&envelope, 0));

    check("retire next sequence",
        gles_submission_envelope_retire(&envelope, 44));

    printf("RESULT passed=%u failed=%u\n", passed, failed);
    return failed ? 1 : 0;
}