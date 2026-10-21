/*
 * Experimental NVN command-list decoder for GLES development.
 *
 * This module does not execute GPU commands.
 * It validates and counts recorded NVN operations.
 *
 * Do not use it as a replacement for backend_execute().
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../nvn.h"
#include "h1_real_draw_capture.h"
#include "h1_draw_state_capture.h"
#include "h1_guest_memory_read.h"
#include "h1_indexed_draw_test.h"
#include "h1_texture_trace.h"
#include "h1_descriptor_probe.h"
#ifdef __ANDROID__
#include "record_coordinator.h"
#endif

#define GLES_EXEC_MAX_RECORDS 100000u
#define GLES_EXEC_MAX_DEPTH 8
#define GLES_EXEC_MAX_CALLS 4096u
#define GLES_EXEC_MAX_RECORD_SIZE (16u * 1024u * 1024u)

typedef struct {
    uint64_t op_counts[CMD_COUNT];
    uint64_t total_records;
    uint64_t unsupported_records;
    uint64_t remaining_records;
    uint64_t remaining_calls;
    bool failed;
} GlesExecStats;

typedef enum {
    GLES_COMMAND_UNSUPPORTED = 0,
    GLES_COMMAND_SUCCESS = 1,
    GLES_COMMAND_FAILED = -1
} GlesCommandResult;

typedef GlesCommandResult (*GlesCommandHandler)(
    const CmdRec *record);

static bool gles_exec_list(
    uint64_t handle,
    unsigned depth,
    GlesExecStats *stats,
    GlesCommandHandler handler)
{
    uint64_t p = handle;

    if (!p || depth > GLES_EXEC_MAX_DEPTH) {
        stats->failed = true;
        return false;
    }

    while (stats->remaining_records > 0) {
        stats->remaining_records--;

        /*
         * This is an internal decoder for trusted, mapped
         * command buffers. It is not a memory-safe parser
         * for arbitrary external input.
         */
        if (p > UINTPTR_MAX ||
            (p & (_Alignof(CmdRec) - 1u)) != 0) {
            stats->failed = true;
            return false;
        }

        const CmdRec *r = (const CmdRec *)(uintptr_t)p;

        if (r->size < sizeof(CmdRec) ||
            (r->size & 7u) != 0 ||
            r->size > GLES_EXEC_MAX_RECORD_SIZE ||
            r->op >= CMD_COUNT) {
            stats->failed = true;
            return false;
        }

        stats->total_records++;
        stats->op_counts[r->op]++;

        switch (r->op) {
        case CMD_END:
            return true;

        case CMD_JUMP:
            if (!r->x[0]) {
                stats->failed = true;
                return false;
            }
            p = r->x[0];
            continue;

        case CMD_CALL: {
            const uint64_t count = r->x[0];
            const size_t extra_size = r->size - sizeof(CmdRec);

            if (count > extra_size / sizeof(uint64_t) ||
                count > stats->remaining_calls ||
                depth >= GLES_EXEC_MAX_DEPTH) {
                stats->failed = true;
                return false;
            }

            stats->remaining_calls -= count;

            const uint64_t *handles =
                (const uint64_t *)r->extra;

            for (uint64_t i = 0; i < count; i++) {
                if (!gles_exec_list(handles[i], depth + 1, stats, handler))
                    return false;
            }

            break;
        }

        default:
            /*
             * A decoded command is not a supported command.
             * No GPU operation is performed here.
             */
            if (handler) {
                GlesCommandResult result = handler(r);

                if (result == GLES_COMMAND_SUCCESS)
                    break;

                if (result == GLES_COMMAND_FAILED) {
                    stats->failed = true;
                    return false;
                }
            }

            stats->unsupported_records++;
            break;
        }

        if (p > UINT64_MAX - r->size) {
            stats->failed = true;
            return false;
        }

        p += r->size;
    }

    stats->failed = true;
    return false;
}

bool gles_decode_command_list(
    uint64_t handle,
    GlesExecStats *out)
{
    if (!out)
        return false;

    memset(out, 0, sizeof(*out));

    out->remaining_records = GLES_EXEC_MAX_RECORDS;
    out->remaining_calls = GLES_EXEC_MAX_CALLS;

    bool ok = gles_exec_list(handle, 0, out, NULL);

    out->failed = !ok;
    return ok;
}

void gles_print_command_stats(const GlesExecStats *stats)
{
    if (!stats)
        return;

    fprintf(stderr,
            "[GLES-EXEC] Records: %llu\n",
            (unsigned long long)stats->total_records);

    fprintf(stderr,
            "[GLES-EXEC] Unsupported: %llu\n",
            (unsigned long long)stats->unsupported_records);

    fprintf(stderr,
            "[GLES-EXEC] Status: %s\n",
            stats->failed ? "FAILED" : "DECODED");

    for (unsigned i = 0; i < CMD_COUNT; i++) {
        if (stats->op_counts[i]) {
            fprintf(stderr,
                    "[GLES-EXEC] Opcode %u: %llu\n",
                    i,
                    (unsigned long long)stats->op_counts[i]);
        }
    }
}
extern bool gles_execute_clear_color(const CmdRec *record);
extern bool gles_execute_set_render_targets(const CmdRec *record);

static GlesCommandResult gles_clear_handler(
    const CmdRec *record)
{
    if (record->op == CMD_SET_RENDER_TARGETS)
        return gles_execute_set_render_targets(record)
            ? GLES_COMMAND_SUCCESS
            : GLES_COMMAND_FAILED;

    if (record->op == CMD_CLEAR_COLOR)
        return gles_execute_clear_color(record)
            ? GLES_COMMAND_SUCCESS
            : GLES_COMMAND_FAILED;

    return GLES_COMMAND_UNSUPPORTED;
}

bool gles_execute_test_command_list(
    uint64_t handle,
    GlesExecStats *out)
{
    if (!out)
        return false;

    memset(out, 0, sizeof(*out));

    out->remaining_records = GLES_EXEC_MAX_RECORDS;
    out->remaining_calls = GLES_EXEC_MAX_CALLS;

    bool ok = gles_exec_list(handle, 0, out,
                             gles_clear_handler);

    out->failed = !ok;
    return ok;
}

/*
 * Diagnostic-only entry point for actual guest command lists.
 * This does not execute graphics operations or signal GPU completion.
 */
/*
 * E127 real render-command compatibility.
 * Diagnostic-only: never invokes OpenGL ES.
 *
 * The real submission path currently processes one command list
 * at a time on the SDL UI thread.
 */
typedef struct {
    uint64_t rt_total;
    uint64_t rt_layout_compatible;
    /* E129 render-target reasons */
    uint64_t rt_short;
    uint64_t rt_not_single;
    uint64_t rt_missing_color;
    uint64_t rt_has_view;
    uint64_t rt_has_depth;
    uint64_t rt_extra_color;
    uint64_t rt_zero_color;
    uint64_t rt_multi_color;
    /* E131 framebuffer configuration */
    uint64_t rt_color_only;
    uint64_t rt_color_depth;
    uint64_t rt_depth_only;
    uint64_t rt_no_attachments;
    uint64_t rt_color_view;
    uint64_t rt_depth_view;
    uint64_t rt_color_view_setmask;
    uint64_t rt_depth_view_setmask;
    uint64_t clear_total;
    uint64_t clear_layout_compatible;
    uint64_t clear_partial_mask;
    uint64_t clear_nonfloat;
    uint64_t clear_other_attachment;
    uint64_t clear_short_record;
} E127RenderStats;

static E127RenderStats e127_render_stats;

typedef struct {
    uint64_t tex[8];
    NvnTextureView views[8];
    uint8_t has_view[8];
    uint64_t depth;
    NvnTextureView dview;
    uint8_t has_dview;
} E127RtRecord;

typedef struct {
    uint32_t depth_enable;
    uint32_t stencil_mask;
    uint32_t stencil_value;
    float depth_value;
} E162DepthClear;

/* E167C3_EXECUTION_ORDER
 * Decoder-only state. Pointer values are opaque identities.
 * No host texture dereferences or graphics operations.
 */
typedef struct {
    uint64_t step;
    uint64_t rt_count;
    uint64_t depth_clear_count;
    uint64_t color_pointer;
    uint64_t depth_pointer;
    unsigned color_count;
    bool has_target;
} E167C3State;

static E167C3State e167c3_state;
static E162DepthClear e162_depth_clear;
static int e162_depth_ready;

static H1RealDrawCapture h1_real_draw_capture;
static H1DrawState h1_draw_state;

#ifdef __ANDROID__
static void h1_capture_first_geometry(
    const H1DrawState *state,
    const CmdRec *r)
{
    if (!state || !r || r->op != CMD_DRAW_ELEMENTS)
        return;

    if (!(state->vertex_buffer_valid & 1u))
        return;

    const H1VertexRange *vb = &state->vertex_buffers[0];

    const uint32_t index_type = (uint32_t)r->x[1];
    const uint32_t index_count = (uint32_t)r->x[2];
    const uint64_t index_address = r->x[3];

    /*
     * Diagnostic scope: the first observed 4-vertex,
     * 6-index draw using 32-bit indices.
     *
     * No hardcoded guest addresses.
     */
    if (index_type != 2 || index_count != 6 ||
        vb->size < 64 || vb->address == 0 ||
        index_address == 0)
        return;

    uint8_t vertices[64] = {0};
    uint32_t indices[6] = {0};

    bool vertex_ok = h1_guest_memory_read(
        vb->address, vertices, sizeof(vertices));

    bool index_ok = h1_guest_memory_read(
        index_address, indices, sizeof(indices));

    fprintf(stderr,
        "[h1-geometry] vertex_read=%d index_read=%d\n",
        vertex_ok ? 1 : 0,
        index_ok ? 1 : 0);

    if (!vertex_ok || !index_ok)
        return;
    h1_test_indexed_draw(vertices, indices);

    for (unsigned i = 0; i < 4; ++i) {
        float a[2], b[2];

        memcpy(a, vertices + i * 16, sizeof(a));
        memcpy(b, vertices + i * 16 + 8, sizeof(b));

        fprintf(stderr,
            "[h1-geometry] vertex=%u "
            "attr0=(%.6f,%.6f) "
            "attr1=(%.6f,%.6f)\n",
            i, a[0], a[1], b[0], b[1]);
    }

    fprintf(stderr,
        "[h1-geometry] indices=%u,%u,%u,%u,%u,%u\n",
        indices[0], indices[1], indices[2],
        indices[3], indices[4], indices[5]);
}
#endif
static GlesCommandResult e127_inspect_render_command(
    const CmdRec *r)
{
    /*
     * H1-15T10: observe real indexed draws only.
     * The command remains unsupported by this diagnostic backend.
     */
    h1_observe_draw_state(&h1_draw_state, r);
    h1_trace_texture_record(r);
#ifdef __ANDROID__
    h1_probe_descriptor_record(r);
#endif
#ifdef __ANDROID__
    if (r->op == CMD_DRAW_ELEMENTS &&
        !h1_real_draw_capture.first_captured)
        h1_capture_first_geometry(&h1_draw_state, r);
#endif
    h1_capture_real_draw(&h1_real_draw_capture, r);
    if (r->op == CMD_SET_RENDER_TARGETS) {
#ifdef __ANDROID__
        /*
         * R8D-4D2: diagnostic coverage of the live RT record
         * identity bridge during actual CMD_CALL/JUMP traversal.
         *
         * The key is looked up by the current record address.
         * Address reuse may make it different from the original
         * recording generation. Never authorize GPU execution
         * from this result.
         */
        {
            extern bool e167d3g4jr8d4_lookup_record_key(
                uintptr_t record_address,
                GlesRecordCoordinatorKey *out);

            static unsigned r8d4d2_total;
            static unsigned r8d4d2_found;
            static unsigned r8d4d2_missing;

            GlesRecordCoordinatorKey key = {0};

            bool found = e167d3g4jr8d4_lookup_record_key(
                (uintptr_t)r, &key);

            unsigned total = ++r8d4d2_total;

            if (found)
                ++r8d4d2_found;
            else
                ++r8d4d2_missing;

            if (total <= 64u) {
                fprintf(stderr,
                    "[R8D4D2] rt=%u record=0x%llx "
                    "found=%u generation=%llu "
                    "found_total=%u missing_total=%u\n",
                    total,
                    (unsigned long long)(uintptr_t)r,
                    found ? 1u : 0u,
                    (unsigned long long)key.generation,
                    r8d4d2_found,
                    r8d4d2_missing);
            }
        }
#endif
        e127_render_stats.rt_total++;

        if (r->size < sizeof(CmdRec) + sizeof(E127RtRecord)) {
            e127_render_stats.rt_short++;
        }
        else {
            E127RtRecord rt;
            memcpy(&rt, r->extra, sizeof(rt));
            /* E167C3: update state in actual decoder traversal order. */
            e167c3_state.step++;
            e167c3_state.rt_count++;
            e167c3_state.color_count = (unsigned)r->x[0];
            e167c3_state.color_pointer = rt.tex[0];
            e167c3_state.depth_pointer = rt.depth;
            e167c3_state.has_target = true;

            if (e167c3_state.rt_count <= 64u) {
                LOG("[E167C3] step=%llu RT colors=%u "
                    "color_ptr=0x%llx depth_ptr=0x%llx "
                    "color_view=%u depth_view=%u\n",
                    (unsigned long long)e167c3_state.step,
                    e167c3_state.color_count,
                    (unsigned long long)e167c3_state.color_pointer,
                    (unsigned long long)e167c3_state.depth_pointer,
                    (unsigned)rt.has_view[0],
                    (unsigned)rt.has_dview);
            }


            if (r->x[0] == 0)
                e127_render_stats.rt_zero_color++;

            if (r->x[0] > 1)
                e127_render_stats.rt_multi_color++;

            if (r->x[0] != 1)
                e127_render_stats.rt_not_single++;

            if (!rt.tex[0])
                e127_render_stats.rt_missing_color++;

            if (rt.has_view[0])
                e127_render_stats.rt_has_view++;

            if (rt.depth || rt.has_dview)
                e127_render_stats.rt_has_depth++;

            bool extra = false;
            for (unsigned i = 1; i < 8; i++) {
                if (rt.tex[i] || rt.has_view[i])
                    extra = true;
            }

            if (extra)
                e127_render_stats.rt_extra_color++;

            /* E131 framebuffer configuration */
            bool has_color = r->x[0] > 0 && rt.tex[0] != 0;
            bool has_depth = rt.depth != 0;

            if (has_color && has_depth)
                e127_render_stats.rt_color_depth++;
            else if (has_color)
                e127_render_stats.rt_color_only++;
            else if (has_depth)
                e127_render_stats.rt_depth_only++;
            else
                e127_render_stats.rt_no_attachments++;

            /* E134 texture-view parameter probe.
             * Log copied command data only. No texture dereference,
             * GPU resource creation, or GL calls.
             */
            if (rt.has_view[0]) {
                const NvnTextureView *v = &rt.views[0];

                if (e127_render_stats.rt_color_view < 12) {
                    fprintf(stderr,
                        "[E134] COLOR view=%llu mask=%#x "
                        "base=%u levels=%u layer=%u layers=%u "
                        "format=%#x target=%#x ds=%#x "
                        "swizzle=%u,%u,%u,%u\n",
                        (unsigned long long)
                            e127_render_stats.rt_color_view,
                        (unsigned)v->setmask,
                        (unsigned)v->base_level,
                        (unsigned)v->num_levels,
                        (unsigned)v->min_layer,
                        (unsigned)v->num_layers,
                        (unsigned)v->format,
                        (unsigned)v->target,
                        (unsigned)v->ds_mode,
                        (unsigned)v->swizzle[0],
                        (unsigned)v->swizzle[1],
                        (unsigned)v->swizzle[2],
                        (unsigned)v->swizzle[3]);
                }

                e127_render_stats.rt_color_view++;

                if (v->setmask)
                    e127_render_stats.rt_color_view_setmask++;
            }

            if (rt.has_dview) {
                const NvnTextureView *v = &rt.dview;

                if (e127_render_stats.rt_depth_view < 12) {
                    fprintf(stderr,
                        "[E134] DEPTH view=%llu mask=%#x "
                        "base=%u levels=%u layer=%u layers=%u "
                        "format=%#x target=%#x ds=%#x "
                        "swizzle=%u,%u,%u,%u\n",
                        (unsigned long long)
                            e127_render_stats.rt_depth_view,
                        (unsigned)v->setmask,
                        (unsigned)v->base_level,
                        (unsigned)v->num_levels,
                        (unsigned)v->min_layer,
                        (unsigned)v->num_layers,
                        (unsigned)v->format,
                        (unsigned)v->target,
                        (unsigned)v->ds_mode,
                        (unsigned)v->swizzle[0],
                        (unsigned)v->swizzle[1],
                        (unsigned)v->swizzle[2],
                        (unsigned)v->swizzle[3]);
                }

                e127_render_stats.rt_depth_view++;

                if (v->setmask)
                    e127_render_stats.rt_depth_view_setmask++;
            }

            if (r->x[0] == 1 &&
                rt.tex[0] &&
                !rt.has_view[0] &&
                !rt.depth &&
                !rt.has_dview &&
                !extra)
                e127_render_stats.rt_layout_compatible++;
        }
    }
    else if (r->op == CMD_CLEAR_COLOR) {
        e127_render_stats.clear_total++;

        /* E161B: inspect real recorded values, no GPU execution. */
        if (e127_render_stats.clear_total <= 12 &&
            r->size >= sizeof(CmdRec) + 16u) {
            uint32_t bits[4] = {0};
            memcpy(bits, r->extra, sizeof(bits));
            LOG("[E161B] COLOR seq=%llu attachment=%llu mask=0x%llx type=%llu bits=%08x,%08x,%08x,%08x\n",
                (unsigned long long)e127_render_stats.clear_total,
                (unsigned long long)r->x[0],
                (unsigned long long)r->x[2],
                (unsigned long long)r->x[5],
                bits[0], bits[1], bits[2], bits[3]);
        }

        if (r->size < sizeof(CmdRec) + 16u) {
            e127_render_stats.clear_short_record++;
        }
        else {
            if (r->x[0] != 0)
                e127_render_stats.clear_other_attachment++;

            if (r->x[5] != 0)
                e127_render_stats.clear_nonfloat++;

            if ((r->x[2] & 0xFu) != 0xFu)
                e127_render_stats.clear_partial_mask++;

            if (r->x[0] == 0 &&
                r->x[5] == 0 &&
                (r->x[2] & 0xFu) == 0xFu)
                e127_render_stats.clear_layout_compatible++;
        }
    }

    if (r->op == CMD_CLEAR_DEPTH_STENCIL) {
        /* E167C3: depth clear uses the most recently traversed RT. */
        e167c3_state.step++;
        e167c3_state.depth_clear_count++;

        if (e167c3_state.depth_clear_count <= 32u) {
            LOG("[E167C3] step=%llu CLEAR_DEPTH "
                "rt_seen=%u depth_ptr=0x%llx "
                "depth_enable=%llu value=%f "
                "stencil_mask=0x%llx\n",
                (unsigned long long)e167c3_state.step,
                e167c3_state.has_target ? 1u : 0u,
                (unsigned long long)e167c3_state.depth_pointer,
                (unsigned long long)r->x[0],
                (double)r->f[0],
                (unsigned long long)r->x[2]);
        }

        /*
         * E162: first real recorded depth-clear values only.
         * This is a diagnostic sample, not command replay.
         */
        int e162_expected = 0;
        if (__atomic_compare_exchange_n(
                &e162_depth_ready, &e162_expected, 1, false,
                __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            e162_depth_clear.depth_enable = r->x[0] != 0;
            e162_depth_clear.depth_value = r->f[0];
            e162_depth_clear.stencil_value = (uint32_t)r->x[1];
            e162_depth_clear.stencil_mask = (uint32_t)r->x[2];
            __atomic_store_n(&e162_depth_ready, 2,
                             __ATOMIC_RELEASE);
        }

        static unsigned e161b_depth_count;
        if (e161b_depth_count < 12) {
            ++e161b_depth_count;
            LOG("[E161B] DEPTH seq=%u depth_enable=%llu depth_value=%f stencil_value=0x%llx stencil_mask=0x%llx\n",
                e161b_depth_count,
                (unsigned long long)r->x[0],
                (double)r->f[0],
                (unsigned long long)r->x[1],
                (unsigned long long)r->x[2]);
        }
    }

    return GLES_COMMAND_UNSUPPORTED;
}

/*
 * E162: diagnostic handoff of recorded Zelda depth-clear parameters.
 * The decoder copies values; the UI never dereferences guest textures.
 */


bool e162_get_depth_clear(E162DepthClear *out)
{
    if (!out ||
        __atomic_load_n(&e162_depth_ready, __ATOMIC_ACQUIRE) != 2)
        return false;

    *out = e162_depth_clear;
    return true;
}

bool gles_diagnose_command_list(uint64_t handle)
{
    GlesExecStats stats;

    memset(&e127_render_stats, 0, sizeof(e127_render_stats));
    /* E167C3: reset association for each diagnostic submission. */
    memset(&e167c3_state, 0, sizeof(e167c3_state));
    memset(&h1_draw_state, 0, sizeof(h1_draw_state));
    memset(&h1_real_draw_capture, 0, sizeof(h1_real_draw_capture));


    /*
     * Reuse the existing bounded traversal and nested CALL/JUMP
     * handling, but inspect instead of executing commands.
     */
    memset(&stats, 0, sizeof(stats));
    stats.remaining_records = GLES_EXEC_MAX_RECORDS;
    stats.remaining_calls = GLES_EXEC_MAX_CALLS;

    bool ok = gles_exec_list(
        handle, 0, &stats, e127_inspect_render_command);

    stats.failed = !ok;
    gles_print_command_stats(&stats);

    fprintf(stderr,
        "[E127] RT total=%llu layout-compatible=%llu\n",
        (unsigned long long)e127_render_stats.rt_total,
        (unsigned long long)e127_render_stats.rt_layout_compatible);

    fprintf(stderr,
        "[E129] RT short=%llu zero-color=%llu multi-color=%llu "
        "not-single=%llu missing-color=%llu\n",
        (unsigned long long)e127_render_stats.rt_short,
        (unsigned long long)e127_render_stats.rt_zero_color,
        (unsigned long long)e127_render_stats.rt_multi_color,
        (unsigned long long)e127_render_stats.rt_not_single,
        (unsigned long long)e127_render_stats.rt_missing_color);

    fprintf(stderr,
        "[E129] RT has-view=%llu has-depth=%llu extra-color=%llu\n",
        (unsigned long long)e127_render_stats.rt_has_view,
        (unsigned long long)e127_render_stats.rt_has_depth,
        (unsigned long long)e127_render_stats.rt_extra_color);

    fprintf(stderr,
        "[E131] color-only=%llu color+depth=%llu "
        "depth-only=%llu no-attachments=%llu\n",
        (unsigned long long)e127_render_stats.rt_color_only,
        (unsigned long long)e127_render_stats.rt_color_depth,
        (unsigned long long)e127_render_stats.rt_depth_only,
        (unsigned long long)e127_render_stats.rt_no_attachments);

    fprintf(stderr,
        "[E131] color-view=%llu depth-view=%llu "
        "color-view-setmask=%llu depth-view-setmask=%llu\n",
        (unsigned long long)e127_render_stats.rt_color_view,
        (unsigned long long)e127_render_stats.rt_depth_view,
        (unsigned long long)e127_render_stats.rt_color_view_setmask,
        (unsigned long long)e127_render_stats.rt_depth_view_setmask);

    fprintf(stderr,
        "[E127] CLEAR total=%llu layout-compatible=%llu\n",
        (unsigned long long)e127_render_stats.clear_total,
        (unsigned long long)e127_render_stats.clear_layout_compatible);

    fprintf(stderr,
        "[E127] CLEAR partial-mask=%llu nonfloat=%llu "
        "other-attachment=%llu short=%llu\n",
        (unsigned long long)e127_render_stats.clear_partial_mask,
        (unsigned long long)e127_render_stats.clear_nonfloat,
        (unsigned long long)e127_render_stats.clear_other_attachment,
        (unsigned long long)e127_render_stats.clear_short_record);

    return ok;
}