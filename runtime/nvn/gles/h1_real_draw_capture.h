#ifndef H1_REAL_DRAW_CAPTURE_H
#define H1_REAL_DRAW_CAPTURE_H

/*
 * H1-15T8: diagnostic-only NVN indexed-draw capture.
 *
 * Does not dereference guest buffer addresses.
 * Does not execute GPU commands.
 * Does not alter command-list state.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../nvn.h"

typedef struct {
    uint64_t indexed_draws;
    bool first_captured;
} H1RealDrawCapture;

static inline void h1_capture_real_draw(
    H1RealDrawCapture *capture,
    const CmdRec *r)
{
    if (!capture || !r || r->op != CMD_DRAW_ELEMENTS)
        return;

    capture->indexed_draws++;

    if (capture->first_captured)
        return;

    capture->first_captured = true;

    const uint32_t mode = (uint32_t)r->x[0];
    const uint32_t index_type = (uint32_t)r->x[1];
    const uint32_t count = (uint32_t)r->x[2];
    const uint64_t index_address = r->x[3];
    const int32_t base_vertex = (int32_t)r->x[4];
    const uint32_t base_instance = (uint32_t)r->x[5];
    const uint32_t instances =
        r->x[6] ? (uint32_t)r->x[6] : 1u;

    fprintf(stderr,
        "[h1-real-draw] first_indexed "
        "mode=%u index_type=%u count=%u "
        "index_address=0x%016" PRIx64 " "
        "base_vertex=%" PRId32 " "
        "base_instance=%u instances=%u\n",
        mode,
        index_type,
        count,
        index_address,
        base_vertex,
        base_instance,
        instances);
}

#endif