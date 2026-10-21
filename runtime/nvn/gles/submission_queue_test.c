#include "submission_queue.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint64_t sequences[GLES_SUBMISSION_CAPACITY];

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        assert(gles_submission_enqueue(1000 + i, &sequences[i]));
    }

    uint64_t extra = 0;
    assert(!gles_submission_enqueue(9999, &extra));

    assert(!gles_submission_reset());

    for (unsigned i = 0; i < GLES_SUBMISSION_CAPACITY; i++) {
        GlesSubmission request = {0};

        assert(gles_submission_dequeue(&request));
        assert(request.command_handle == 1000 + i);
        assert(request.sequence == sequences[i]);

        GlesSubmissionState state;
        assert(gles_submission_query(request.sequence, &state));
        assert(state == GLES_SUBMISSION_PENDING);

        assert(gles_submission_finish(request.sequence, true));

        assert(gles_submission_query(request.sequence, &state));
        assert(state == GLES_SUBMISSION_COMPLETED);
    }

    GlesSubmission empty = {0};
    assert(!gles_submission_dequeue(&empty));

    // Completed slots cannot currently be reused without reset.
    assert(!gles_submission_enqueue(7777, &extra));

    assert(gles_submission_reset());

    assert(gles_submission_enqueue(7777, &extra));
    assert(extra > sequences[GLES_SUBMISSION_CAPACITY - 1]);

    puts("E89: all submission queue assertions passed");
    return 0;
}