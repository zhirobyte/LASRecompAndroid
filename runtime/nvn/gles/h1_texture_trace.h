#ifndef H1_TEXTURE_TRACE_H
#define H1_TEXTURE_TRACE_H

#include "../nvn.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    uint64_t records;
    uint64_t texture_binds;
    uint64_t texture_pool_sets;
    uint64_t sampler_pool_sets;
    uint64_t program_binds;
    uint64_t current_program;
    uint64_t last_texture_x[7];
    uint32_t last_texture_size;
    bool have_texture;
    bool first_draw;
} H1TextureTrace;

static H1TextureTrace h1_texture_trace;

static void h1_trace_texture_record(const CmdRec *r)
{
    if (!r)
        return;

    bool interesting = false;
    const char *kind = "unknown";

    switch (r->op) {
    case CMD_BIND_TEXTURES:
        h1_texture_trace.texture_binds++;
        h1_texture_trace.have_texture = true;
        h1_texture_trace.last_texture_size = r->size;

        for (unsigned i = 0; i < 7; ++i)
            h1_texture_trace.last_texture_x[i] = r->x[i];

        kind = "BIND_TEXTURES";
        interesting = true;
        break;

    case CMD_SET_TEXTURE_POOL:
        h1_texture_trace.texture_pool_sets++;
        kind = "SET_TEXTURE_POOL";
        interesting = true;
        break;

    case CMD_SET_SAMPLER_POOL:
        h1_texture_trace.sampler_pool_sets++;
        kind = "SET_SAMPLER_POOL";
        interesting = true;
        break;

    case CMD_BIND_PROGRAM:
        h1_texture_trace.program_binds++;
        h1_texture_trace.current_program = r->x[0];
        kind = "BIND_PROGRAM";
        interesting = true;
        break;

    default:
        break;
    }

    if (interesting) {
        h1_texture_trace.records++;

        if (h1_texture_trace.records <= 12) {
            fprintf(stderr,
                "[h1-texture] kind=%s op=%u size=%u "
                "x0=%" PRIu64 " x1=%" PRIu64 " "
                "x2=%" PRIu64 " x3=%" PRIu64 " "
                "x4=%" PRIu64 " x5=%" PRIu64 " "
                "x6=%" PRIu64 "\n",
                kind, r->op, r->size,
                r->x[0], r->x[1], r->x[2],
                r->x[3], r->x[4], r->x[5],
                r->x[6]);

            /*
             * Bounded raw payload preview.
             * No pointer dereferencing.
             */
            if (r->op == CMD_BIND_TEXTURES &&
                r->size >= sizeof(CmdRec)) {

                uint64_t extra_size =
                    (uint64_t)r->size - sizeof(CmdRec);

                unsigned preview =
                    extra_size < 32 ? (unsigned)extra_size : 32u;

                fprintf(stderr,
                    "[h1-texture] payload_bytes=%" PRIu64
                    " preview=",
                    extra_size);

                for (unsigned i = 0; i < preview; ++i)
                    fprintf(stderr, "%02x", r->extra[i]);

                fprintf(stderr, "\n");
            }
        }
    }

    if (r->op == CMD_DRAW_ELEMENTS &&
        !h1_texture_trace.first_draw) {

        h1_texture_trace.first_draw = true;

        fprintf(stderr,
            "[h1-texture] FIRST_DRAW "
            "program=0x%016" PRIx64 " "
            "texture_binds=%" PRIu64 " "
            "texture_pool_sets=%" PRIu64 " "
            "sampler_pool_sets=%" PRIu64 " "
            "have_texture=%u\n",
            h1_texture_trace.current_program,
            h1_texture_trace.texture_binds,
            h1_texture_trace.texture_pool_sets,
            h1_texture_trace.sampler_pool_sets,
            h1_texture_trace.have_texture ? 1u : 0u);

        if (h1_texture_trace.have_texture) {
            fprintf(stderr,
                "[h1-texture] LAST_TEXTURE_BIND "
                "size=%u x0=%" PRIu64
                " x1=%" PRIu64
                " x2=%" PRIu64
                " x3=%" PRIu64 "\n",
                h1_texture_trace.last_texture_size,
                h1_texture_trace.last_texture_x[0],
                h1_texture_trace.last_texture_x[1],
                h1_texture_trace.last_texture_x[2],
                h1_texture_trace.last_texture_x[3]);
        }
    }
}

#endif