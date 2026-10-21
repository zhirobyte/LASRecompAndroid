#include "submission_queue.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint64_t ids[GLES_SUBMISSION_CAPACITY] = {0};
    GlesSubmission request = {0};
    GlesSubmissionState state;

    // Fill the entire queue.
    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        assert(gles_submission_enqueue(1000 + i, &ids[i]));
    }

    // Pending entries must not be retired.
    assert(!gles_submission_retire(ids[0]));

    // Queue capacity must be enforced.
    uint64_t extra = 0;
    assert(!gles_submission_enqueue(9999, &extra));

    // Process the first entry.
    assert(gles_submission_dequeue(&request));
    assert(request.sequence == ids[0]);
    assert(request.command_handle == 1000);

    assert(!gles_submission_retire(request.sequence));

    assert(gles_submission_finish(request.sequence, true));
    assert(gles_submission_query(request.sequence, &state));
    assert(state == GLES_SUBMISSION_COMPLETED);

    assert(gles_submission_retire(request.sequence));

    // Double retirement and queries of retired IDs must fail.
    assert(!gles_submission_retire(request.sequence));
    assert(!gles_submission_query(request.sequence, &state));

    // Reuse the freed slot.
    uint64_t reused = 0;
    assert(gles_submission_enqueue(7777, &reused));
    assert(reused > ids[GLES_SUBMISSION_CAPACITY - 1]);

    // Finish and retire all remaining original requests.
    for (unsigned i = 1; i < GLES_SUBMISSION_CAPACITY; i++) {
        assert(gles_submission_dequeue(&request));
        assert(request.sequence == ids[i]);

        bool success = (i % 2) == 0;

        assert(gles_submission_finish(request.sequence, success));
        assert(gles_submission_query(request.sequence, &state));

        assert(state == (success
            ? GLES_SUBMISSION_COMPLETED
            : GLES_SUBMISSION_FAILED));

        assert(gles_submission_retire(request.sequence));
    }

    // The reused submission should remain queued.
    assert(gles_submission_dequeue(&request));
    assert(request.sequence == reused);
    assert(request.command_handle == 7777);

    assert(gles_submission_finish(request.sequence, true));
    assert(gles_submission_retire(request.sequence));

    assert(!gles_submission_dequeue(&request));

    // Sustained reuse: process 10,000 more submissions.
    uint64_t previous = reused;

    for (unsigned i = 0; i < 10000; i++) {
        uint64_t sequence = 0;

        assert(gles_submission_enqueue(20000 + i, &sequence));
        assert(sequence > previous);

        assert(gles_submission_dequeue(&request));
        assert(request.sequence == sequence);
        assert(request.command_handle == 20000 + i);

        assert(gles_submission_finish(sequence, true));
        assert(gles_submission_retire(sequence));

        previous = sequence;
    }

    assert(gles_submission_reset());

    puts("E92 PASS: retirement, slot reuse, and 10000 submissions");
    return 0;
}