#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
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

typedef struct {
    uint32_t op, size;
    uint64_t x[7];
    float f[4];
    float color[4];
} ClearRecord;

typedef struct {
    uint32_t op, size;
    uint64_t x[7];
    float f[4];
} EndRecord;

typedef struct {
    ClearRecord clear;
    EndRecord end;
} CommandList;

_Static_assert(sizeof(EndRecord) == sizeof(CmdRec),
               "Unexpected NVN record size");

void gles_run_dispatch_selftest(void)
{
    GLint old_fbo = 0, old_texture = 0;
    GLint old_read_fbo = 0;
    GLboolean old_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean old_mask[4] = {0};

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_fbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_fbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    glGetBooleanv(GL_COLOR_WRITEMASK, old_mask);

    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    bool pass = false;

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE) {

        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        CommandList commands = {0};

        commands.clear.op = CMD_CLEAR_COLOR;
        commands.clear.size = sizeof(ClearRecord);
        commands.clear.x[0] = 0;
        commands.clear.x[2] = 0xF;
        commands.clear.x[5] = 0;
        commands.clear.color[0] = 0.2f;
        commands.clear.color[1] = 0.4f;
        commands.clear.color[2] = 0.6f;
        commands.clear.color[3] = 1.0f;

        commands.end.op = CMD_END;
        commands.end.size = sizeof(EndRecord);

        GlesExecStats stats;
        bool executed = gles_execute_test_command_list(
            (uint64_t)(uintptr_t)&commands, &stats);

        unsigned char pixel[4] = {0};

        if (executed) {
            glReadPixels(0, 0, 1, 1,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        }

        fprintf(stderr,
                "[GLES-DISPATCH] Pixel: %u %u %u %u\n",
                pixel[0], pixel[1], pixel[2], pixel[3]);

        pass = executed &&
               stats.total_records == 2 &&
               stats.op_counts[CMD_CLEAR_COLOR] == 1 &&
               stats.unsupported_records == 0 &&
               pixel[0] >= 50 && pixel[0] <= 52 &&
               pixel[1] >= 101 && pixel[1] <= 103 &&
               pixel[2] >= 152 && pixel[2] <= 154 &&
               pixel[3] == 255;
    }

    fprintf(stderr, "[GLES-DISPATCH] RESULT: %s\n",
            pass ? "PASS" : "FAIL");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read_fbo);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);

    if (old_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glColorMask(old_mask[0], old_mask[1],
                old_mask[2], old_mask[3]);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}