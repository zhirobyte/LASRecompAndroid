/*
 * Experimental NVN command-list decoder for GLES development.
 *
 * This module does not execute GPU commands.
 * It validates and counts recorded NVN operations.
 *
 * Do not use it as a replacement for backend_execute().
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../nvn.h"

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

static GlesCommandResult gles_clear_handler(
    const CmdRec *record)
{
    if (record->op != CMD_CLEAR_COLOR)
        return GLES_COMMAND_UNSUPPORTED;

    return gles_execute_clear_color(record)
        ? GLES_COMMAND_SUCCESS
        : GLES_COMMAND_FAILED;
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