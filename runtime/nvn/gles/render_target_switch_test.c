#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "../nvn.h"

typedef struct {
    uint64_t op_counts[CMD_COUNT];
    uint64_t total_records;
    uint64_t unsupported_records;
    uint64_t remaining_records;
    uint64_t remaining_calls;
    bool failed;
} GlesExecStats;

extern bool gles_execute_test_command_list(
    uint64_t handle, GlesExecStats *out);

extern bool gles_register_test_render_target(
    const NvnTexture *texture, unsigned gl_texture);

extern void gles_unregister_test_render_target(void);

typedef struct {
    uint64_t tex[8];
    NvnTextureView views[8];
    uint8_t has_view[8];
    uint64_t depth;
    NvnTextureView dview;
    uint8_t has_dview;
} RtPayload;

typedef struct {
    uint32_t op, size;
    uint64_t x[7];
    float f[4];
    RtPayload payload;
} RtCommand;

typedef struct {
    uint32_t op, size;
    uint64_t x[7];
    float f[4];
    float color[4];
} ClearCommand;

typedef struct {
    uint32_t op, size;
    uint64_t x[7];
    float f[4];
} EndCommand;

typedef struct {
    RtCommand target;
    ClearCommand clear;
    EndCommand end;
} CommandList;

_Static_assert(offsetof(RtCommand, payload) == sizeof(CmdRec),
               "Invalid render-target layout");

_Static_assert(offsetof(ClearCommand, color) == sizeof(CmdRec),
               "Invalid clear layout");

static void make_list(
    CommandList *list,
    const NvnTexture *texture,
    float red, float green, float blue)
{
    memset(list, 0, sizeof(*list));

    list->target.op = CMD_SET_RENDER_TARGETS;
    list->target.size = sizeof(RtCommand);
    list->target.x[0] = 1;
    list->target.payload.tex[0] =
        (uint64_t)(uintptr_t)texture;

    list->clear.op = CMD_CLEAR_COLOR;
    list->clear.size = sizeof(ClearCommand);
    list->clear.x[0] = 0;
    list->clear.x[2] = 0xF;
    list->clear.color[0] = red;
    list->clear.color[1] = green;
    list->clear.color[2] = blue;
    list->clear.color[3] = 1.0f;

    list->end.op = CMD_END;
    list->end.size = sizeof(EndCommand);
}

static bool execute_list(CommandList *list)
{
    GlesExecStats stats = {0};

    bool ok = gles_execute_test_command_list(
        (uint64_t)(uintptr_t)list, &stats);

    return ok &&
           !stats.failed &&
           stats.total_records == 3 &&
           stats.unsupported_records == 0 &&
           stats.op_counts[CMD_SET_RENDER_TARGETS] == 1 &&
           stats.op_counts[CMD_CLEAR_COLOR] == 1;
}

static bool bind_only(const NvnTexture *texture)
{
    CommandList list;
    make_list(&list, texture, 0, 0, 0);

    list.target.size = sizeof(RtCommand);

    EndCommand end = {0};
    end.op = CMD_END;
    end.size = sizeof(end);

    struct {
        RtCommand target;
        EndCommand end;
    } commands = {0};

    commands.target = list.target;
    commands.end = end;

    GlesExecStats stats = {0};

    bool ok = gles_execute_test_command_list(
        (uint64_t)(uintptr_t)&commands, &stats);

    return ok && !stats.failed &&
           stats.total_records == 2 &&
           stats.unsupported_records == 0;
}

static bool pixel_equals(
    unsigned char red,
    unsigned char green,
    unsigned char blue)
{
    unsigned char pixel[4] = {0};

    glReadPixels(0, 0, 1, 1,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    return pixel[0] == red &&
           pixel[1] == green &&
           pixel[2] == blue &&
           pixel[3] == 255;
}

void gles_run_render_target_switch_selftest(void)
{
    GLint old_draw = 0;
    GLint old_read = 0;
    GLint old_texture = 0;
    GLboolean old_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean old_mask[4] = {0};

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    glGetBooleanv(GL_COLOR_WRITEMASK, old_mask);

    GLuint textures[2] = {0};
    glGenTextures(2, textures);

    for (unsigned i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     4, 4, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);

        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    NvnTexture nvn[2] = {{0}};

    for (unsigned i = 0; i < 2; i++) {
        nvn[i].width = 4;
        nvn[i].height = 4;
        nvn[i].depth = 1;
        nvn[i].levels = 1;
        nvn[i].samples = 1;
    }

    bool registered_a =
        gles_register_test_render_target(&nvn[0], textures[0]);

    bool registered_b =
        gles_register_test_render_target(&nvn[1], textures[1]);

    bool duplicate_rejected =
        !gles_register_test_render_target(&nvn[0], textures[0]);

    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    CommandList red;
    CommandList blue;

    make_list(&red, &nvn[0], 1.0f, 0.0f, 0.0f);
    make_list(&blue, &nvn[1], 0.0f, 0.0f, 1.0f);

    bool a_ok = registered_a && execute_list(&red);
    bool b_ok = registered_b && execute_list(&blue);

    bool a_retained = a_ok &&
        bind_only(&nvn[0]) &&
        pixel_equals(255, 0, 0);

    bool b_retained = b_ok &&
        bind_only(&nvn[1]) &&
        pixel_equals(0, 0, 255);

    NvnTexture unknown = {0};
    unknown.width = 4;
    unknown.height = 4;
    unknown.depth = 1;
    unknown.levels = 1;
    unknown.samples = 1;

    CommandList invalid;
    make_list(&invalid, &unknown, 0, 1, 0);

    GlesExecStats invalid_stats = {0};

    bool invalid_ok = gles_execute_test_command_list(
        (uint64_t)(uintptr_t)&invalid, &invalid_stats);

    bool invalid_rejected =
        !invalid_ok && invalid_stats.failed &&
        invalid_stats.total_records == 1;

    bool pass =
        registered_a &&
        registered_b &&
        duplicate_rejected &&
        a_ok &&
        b_ok &&
        a_retained &&
        b_retained &&
        invalid_rejected &&
        glGetError() == GL_NO_ERROR;

    fprintf(stderr,
            "[GLES-RT-SWITCH] A RED: %s\n",
            a_retained ? "PASS" : "FAIL");

    fprintf(stderr,
            "[GLES-RT-SWITCH] B BLUE: %s\n",
            b_retained ? "PASS" : "FAIL");

    fprintf(stderr,
            "[GLES-RT-SWITCH] INVALID: %s\n",
            invalid_rejected ? "PASS" : "FAIL");

    fprintf(stderr,
            "[GLES-RT-SWITCH] RESULT: %s\n",
            pass ? "PASS" : "FAIL");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);

    gles_unregister_test_render_target();

    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);

    if (old_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glColorMask(old_mask[0], old_mask[1],
                old_mask[2], old_mask[3]);

    glDeleteTextures(2, textures);
}