#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../nvn.h"

/* Matches the diagnostic structure in exec.c. */
typedef struct {
    uint64_t op_counts[CMD_COUNT];
    uint64_t total_records;
    uint64_t unsupported_records;
    uint64_t remaining_records;
    uint64_t remaining_calls;
    bool failed;
} GlesExecStats;

extern bool gles_decode_command_list(uint64_t handle,
                                     GlesExecStats *out);

extern void gles_print_command_stats(const GlesExecStats *stats);

typedef struct {
    uint32_t op, size;
    uint64_t x[7];
    float f[4];
    uint64_t handle;
} CallRecord;

static bool check(bool condition, const char *name)
{
    fprintf(stderr, "[GLES-SELFTEST] %s: %s\n",
            name, condition ? "PASS" : "FAIL");
    return condition;
}

void gles_run_decoder_selftest(void)
{
    bool all_ok = true;
    GlesExecStats stats;

    /* Test 1: a single END record. */
    CmdRec end = {0};
    end.op = CMD_END;
    end.size = sizeof(CmdRec);

    bool ok = gles_decode_command_list(
        (uint64_t)(uintptr_t)&end, &stats);

    all_ok &= check(ok &&
                    stats.total_records == 1 &&
                    stats.unsupported_records == 0,
                    "END");

    /* Test 2: jump to an END record. */
    CmdRec jump[2] = {0};

    jump[0].op = CMD_JUMP;
    jump[0].size = sizeof(CmdRec);
    jump[0].x[0] = (uint64_t)(uintptr_t)&jump[1];

    jump[1].op = CMD_END;
    jump[1].size = sizeof(CmdRec);

    ok = gles_decode_command_list(
        (uint64_t)(uintptr_t)&jump[0], &stats);

    all_ok &= check(ok &&
                    stats.total_records == 2,
                    "JUMP");

    /* Test 3: CALL a nested command list. */
    CmdRec nested = {0};
    nested.op = CMD_END;
    nested.size = sizeof(CmdRec);

    struct {
        CallRecord call;
        uint32_t end_op, end_size;
        uint64_t end_x[7];
        float end_f[4];
    } list = {0};

    list.call.op = CMD_CALL;
    list.call.size = sizeof(CallRecord);
    list.call.x[0] = 1;
    list.call.handle = (uint64_t)(uintptr_t)&nested;

    list.end_op = CMD_END;
    list.end_size = sizeof(CmdRec);

    ok = gles_decode_command_list(
        (uint64_t)(uintptr_t)&list.call, &stats);

    all_ok &= check(ok &&
                    stats.total_records == 3 &&
                    stats.op_counts[CMD_CALL] == 1,
                    "CALL");

    /* Test 4: recognize but do not execute DRAW_ARRAYS. */
    CmdRec draw[2] = {0};

    draw[0].op = CMD_DRAW_ARRAYS;
    draw[0].size = sizeof(CmdRec);

    draw[1].op = CMD_END;
    draw[1].size = sizeof(CmdRec);

    ok = gles_decode_command_list(
        (uint64_t)(uintptr_t)&draw[0], &stats);

    all_ok &= check(ok &&
                    stats.unsupported_records == 1 &&
                    stats.op_counts[CMD_DRAW_ARRAYS] == 1,
                    "UNSUPPORTED DRAW");

    /* Test 5: reject invalid record size. */
    CmdRec invalid = {0};
    invalid.op = CMD_END;
    invalid.size = 0;

    ok = gles_decode_command_list(
        (uint64_t)(uintptr_t)&invalid, &stats);

    all_ok &= check(!ok && stats.failed,
                    "INVALID RECORD");

    fprintf(stderr,
            "[GLES-SELFTEST] OVERALL: %s\n",
            all_ok ? "PASS" : "FAIL");
}