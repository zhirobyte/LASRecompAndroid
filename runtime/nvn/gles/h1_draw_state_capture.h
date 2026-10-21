#ifndef H1_DRAW_STATE_CAPTURE_H
#define H1_DRAW_STATE_CAPTURE_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../nvn.h"

typedef struct {
    uint64_t address;
    uint64_t size;
} H1VertexRange;

typedef struct {
    uint64_t program;
    H1VertexRange vertex_buffers[16];
    uint32_t vertex_attributes[16];
    uint32_t vertex_streams[16][2];
    uint32_t attribute_count;
    uint32_t stream_count;
    uint16_t vertex_buffer_valid;
    uint64_t render_target_record;
    uint32_t render_target_colors;
    bool first_draw_logged;
} H1DrawState;

static inline void h1_observe_draw_state(
    H1DrawState *state,
    const CmdRec *r)
{
    if (!state || !r)
        return;

    switch (r->op) {
    case CMD_BIND_PROGRAM:
        state->program = r->x[0];
        break;

    /* H1-15T23: capture recorded vertex layout metadata only. */
    case CMD_BIND_VERTEX_ATTRIB: {
        uint64_t count = r->x[0];
        if (count > 16 ||
            (uint64_t)r->size < sizeof(CmdRec) ||
            count > ((uint64_t)r->size - sizeof(CmdRec)) / 4)
            break;
        memset(state->vertex_attributes, 0,
               sizeof(state->vertex_attributes));
        memcpy(state->vertex_attributes, r->extra,
               (size_t)count * 4);
        state->attribute_count = (uint32_t)count;
        break;
    }

    case CMD_BIND_VERTEX_STREAM: {
        uint64_t count = r->x[0];
        if (count > 16 ||
            (uint64_t)r->size < sizeof(CmdRec) ||
            count > ((uint64_t)r->size - sizeof(CmdRec)) / 8)
            break;
        memset(state->vertex_streams, 0,
               sizeof(state->vertex_streams));
        memcpy(state->vertex_streams, r->extra,
               (size_t)count * 8);
        state->stream_count = (uint32_t)count;
        break;
    }

    case CMD_BIND_VERTEX_BUFFERS: {
        uint64_t first = r->x[0];
        uint64_t count = r->x[1];

        if (first >= 16 || count > 16 - first)
            break;

        if ((uint64_t)r->size < sizeof(CmdRec) ||
            count > ((uint64_t)r->size - sizeof(CmdRec)) / 16)
            break;

        for (uint64_t i = 0; i < count; ++i) {
            H1VertexRange range;
            memcpy(&range, r->extra + i * 16, sizeof(range));

            state->vertex_buffers[first + i] = range;
            state->vertex_buffer_valid |=
                (uint16_t)(1u << (first + i));
        }
        break;
    }

    case CMD_SET_RENDER_TARGETS:
        state->render_target_record = (uint64_t)(uintptr_t)r;
        state->render_target_colors = (uint32_t)r->x[0];
        break;

    case CMD_DRAW_ELEMENTS:
        if (state->first_draw_logged)
            break;

        state->first_draw_logged = true;

        fprintf(stderr,
            "[h1-draw-state] first_indexed "
            "program=0x%016" PRIx64 " "
            "rt_record=0x%016" PRIx64 " "
            "rt_colors=%u valid_vbuf_mask=0x%04x\n",
            state->program,
            state->render_target_record,
            state->render_target_colors,
            (unsigned)state->vertex_buffer_valid);

        fprintf(stderr,
            "[h1-vertex-layout] attributes=%u streams=%u\n",
            state->attribute_count, state->stream_count);

        for (unsigned i = 0; i < state->attribute_count; ++i) {
            uint32_t a = state->vertex_attributes[i];
            fprintf(stderr,
                "[h1-vertex-layout] attr=%u fmt=%u stream=%u offset=%u\n",
                i, a & 0xFFu, (a >> 8) & 0x1Fu, a >> 13);
        }

        for (unsigned i = 0; i < state->stream_count; ++i) {
            fprintf(stderr,
                "[h1-vertex-layout] stream=%u stride=%u divisor=%u\n",
                i, state->vertex_streams[i][0],
                state->vertex_streams[i][1]);
        }

        for (unsigned i = 0; i < 16; ++i) {
            if (!(state->vertex_buffer_valid & (1u << i)))
                continue;

            fprintf(stderr,
                "[h1-draw-state] vbuf=%u "
                "address=0x%016" PRIx64 " "
                "size=%" PRIu64 "\n",
                i,
                state->vertex_buffers[i].address,
                state->vertex_buffers[i].size);
        }
        break;

    default:
        break;
    }
}

#endif