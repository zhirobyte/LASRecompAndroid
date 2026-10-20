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
               "Invalid RT command layout");

_Static_assert(offsetof(ClearCommand, color) == sizeof(CmdRec),
               "Invalid clear command layout");

void gles_run_render_target_selftest(void)
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

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    NvnTexture nvn_texture = {0};
    nvn_texture.width = 4;
    nvn_texture.height = 4;
    nvn_texture.depth = 1;
    nvn_texture.levels = 1;
    nvn_texture.samples = 1;

    bool registered = gles_register_test_render_target(
        &nvn_texture, texture);

    CommandList commands = {0};

    commands.target.op = CMD_SET_RENDER_TARGETS;
    commands.target.size = sizeof(RtCommand);
    commands.target.x[0] = 1;
    commands.target.payload.tex[0] =
        (uint64_t)(uintptr_t)&nvn_texture;

    commands.clear.op = CMD_CLEAR_COLOR;
    commands.clear.size = sizeof(ClearCommand);
    commands.clear.x[0] = 0;
    commands.clear.x[2] = 0xF;
    commands.clear.x[5] = 0;
    commands.clear.color[0] = 0.3f;
    commands.clear.color[1] = 0.5f;
    commands.clear.color[2] = 0.7f;
    commands.clear.color[3] = 1.0f;

    commands.end.op = CMD_END;
    commands.end.size = sizeof(EndCommand);

    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    GlesExecStats stats = {0};

    bool executed = registered &&
        gles_execute_test_command_list(
            (uint64_t)(uintptr_t)&commands, &stats);

    unsigned char pixel[4] = {0};

    if (executed) {
        glReadPixels(0, 0, 1, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    }

    bool pass =
        executed &&
        !stats.failed &&
        stats.total_records == 3 &&
        stats.unsupported_records == 0 &&
        stats.op_counts[CMD_SET_RENDER_TARGETS] == 1 &&
        stats.op_counts[CMD_CLEAR_COLOR] == 1 &&
        pixel[0] >= 75 && pixel[0] <= 78 &&
        pixel[1] >= 126 && pixel[1] <= 129 &&
        pixel[2] >= 177 && pixel[2] <= 180 &&
        pixel[3] == 255;

    fprintf(stderr,
            "[GLES-RT] Pixel: %u %u %u %u\n",
            pixel[0], pixel[1], pixel[2], pixel[3]);

    fprintf(stderr,
            "[GLES-RT] RESULT: %s\n",
            pass ? "PASS" : "FAIL");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);

    if (registered)
        gles_unregister_test_render_target();

    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);

    if (old_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glColorMask(old_mask[0], old_mask[1],
                old_mask[2], old_mask[3]);

    glDeleteTextures(1, &texture);
}