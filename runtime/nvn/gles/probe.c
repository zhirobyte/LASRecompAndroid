#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <stdio.h>

void gles_probe_context(void)
{
    const GLubyte *vendor = glGetString(GL_VENDOR);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);
    const GLubyte *glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);

    fprintf(stderr, "\n[GLES-PROBE] === GPU INFORMATION ===\n");

    fprintf(stderr, "[GLES-PROBE] Vendor: %s\n",
            vendor ? (const char *)vendor : "NO CONTEXT");

    fprintf(stderr, "[GLES-PROBE] Renderer: %s\n",
            renderer ? (const char *)renderer : "NO CONTEXT");

    fprintf(stderr, "[GLES-PROBE] Version: %s\n",
            version ? (const char *)version : "NO CONTEXT");

    fprintf(stderr, "[GLES-PROBE] GLSL: %s\n",
            glsl ? (const char *)glsl : "NO CONTEXT");

    if (!version) {
        fprintf(stderr,
                "[GLES-PROBE] No active OpenGL ES context\n");
        return;
    }

    GLint major = 0;
    GLint minor = 0;
    GLint compute_invocations = 0;
    GLint texture_size = 0;

    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_size);

    if (major > 3 || (major == 3 && minor >= 1)) {
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                      &compute_invocations);
    }

    fprintf(stderr,
            "[GLES-PROBE] API: %d.%d\n", major, minor);

    fprintf(stderr,
            "[GLES-PROBE] Max texture: %d\n", texture_size);

    fprintf(stderr,
            "[GLES-PROBE] Max compute invocations: %d\n",
            compute_invocations);

    fprintf(stderr,
            "[GLES-PROBE] === END ===\n\n");
}


void gles_test_framebuffer(void)
{
    GLint previous_fbo = 0;
    GLint previous_viewport[4] = {0};

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);

    GLuint fbo = 0;
    GLuint texture = 0;

    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 4, 4, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           texture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status == GL_FRAMEBUFFER_COMPLETE) {
        glViewport(0, 0, 4, 4);
        glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        unsigned char pixel[4] = {0};

        glReadPixels(0, 0, 1, 1,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     pixel);

        fprintf(stderr,
                "[GLES-TEST] Pixel RGBA: %u %u %u %u\n",
                pixel[0], pixel[1],
                pixel[2], pixel[3]);

        if (pixel[0] >= 62 && pixel[0] <= 66 &&
            pixel[1] >= 126 && pixel[1] <= 129 &&
            pixel[2] >= 190 && pixel[2] <= 193 &&
            pixel[3] == 255) {
            fprintf(stderr,
                    "[GLES-TEST] FRAMEBUFFER CLEAR PASS\n");
        } else {
            fprintf(stderr,
                    "[GLES-TEST] FRAMEBUFFER CLEAR FAIL\n");
        }
    } else {
        fprintf(stderr,
                "[GLES-TEST] FBO incomplete: 0x%x\n",
                status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_fbo);
    glViewport(previous_viewport[0],
               previous_viewport[1],
               previous_viewport[2],
               previous_viewport[3]);

    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &fbo);
}