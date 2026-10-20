#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "../nvn.h"

extern bool gles_execute_clear_color(const CmdRec *r);

typedef struct {
    uint32_t op;
    uint32_t size;
    uint64_t x[7];
    float f[4];
    float color[4];
} ClearRecord;

_Static_assert(offsetof(ClearRecord, color) == sizeof(CmdRec),
               "NVN command payload offset mismatch");

void gles_run_clear_selftest(void)
{
    GLint previous_fbo = 0;
    GLint previous_texture = 0;
    GLint previous_viewport[4] = {0};
    GLint previous_scissor_box[4] = {0};
    GLboolean previous_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean previous_mask[4] = {0};

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, previous_scissor_box);
    glGetBooleanv(GL_COLOR_WRITEMASK, previous_mask);

    GLuint texture = 0;
    GLuint fbo = 0;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 4, 4, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           texture, 0);

    bool pass = false;

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE) {

        glViewport(0, 0, 4, 4);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        ClearRecord command = {0};

        command.op = CMD_CLEAR_COLOR;
        command.size = sizeof(command);
        command.x[0] = 0;
        command.x[2] = 0xF;
        command.x[5] = 0;

        command.color[0] = 0.25f;
        command.color[1] = 0.50f;
        command.color[2] = 0.75f;
        command.color[3] = 1.00f;

        bool executed = gles_execute_clear_color(
            (const CmdRec *)&command);

        unsigned char pixel[4] = {0};

        if (executed) {
            glReadPixels(0, 0, 1, 1,
                         GL_RGBA, GL_UNSIGNED_BYTE,
                         pixel);
        }

        fprintf(stderr,
                "[GLES-NVN-CLEAR] Pixel: %u %u %u %u\n",
                pixel[0], pixel[1],
                pixel[2], pixel[3]);

        pass = executed &&
               pixel[0] >= 62 && pixel[0] <= 66 &&
               pixel[1] >= 126 && pixel[1] <= 129 &&
               pixel[2] >= 190 && pixel[2] <= 193 &&
               pixel[3] == 255;
    }

    fprintf(stderr,
            "[GLES-NVN-CLEAR] RESULT: %s\n",
            pass ? "PASS" : "FAIL");

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_fbo);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);

    glViewport(previous_viewport[0],
               previous_viewport[1],
               previous_viewport[2],
               previous_viewport[3]);

    glScissor(previous_scissor_box[0],
              previous_scissor_box[1],
              previous_scissor_box[2],
              previous_scissor_box[3]);

    if (previous_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glColorMask(previous_mask[0],
                previous_mask[1],
                previous_mask[2],
                previous_mask[3]);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
}