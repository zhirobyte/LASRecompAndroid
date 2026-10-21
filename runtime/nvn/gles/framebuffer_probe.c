/*
 * E140 synthetic GLES framebuffer capability probe.
 * Must run on the SDL UI thread with the GLES context current.
 * No guest texture pointers, command decoding, or gameplay rendering.
 */
#include <GLES3/gl3.h>
#include <stdio.h>

static void e140_check(const char *name)
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GLenum error = glGetError();

    fprintf(stderr,
        "[E140] %s status=0x%x complete=%d error=0x%x\n",
        name,
        (unsigned)status,
        status == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR,
        (unsigned)error);
}

void gles_e140_framebuffer_probe(void)
{
    GLint old_draw = 0;
    GLint old_read = 0;
    GLint old_texture = 0;
    GLint old_active = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);

    GLuint color = 0;
    GLuint depth = 0;
    GLuint fbo = 0;

    glGenTextures(1, &color);
    glGenTextures(1, &depth);
    glGenFramebuffers(1, &fbo);

    if (!color || !depth || !fbo) {
        fprintf(stderr, "[E140] allocation failed error=0x%x\n",
                (unsigned)glGetError());
        goto cleanup;
    }

    glBindTexture(GL_TEXTURE_2D, color);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R11F_G11F_B10F, 64, 64);

    GLenum color_error = glGetError();
    fprintf(stderr, "[E140] color-storage error=0x%x\n",
            (unsigned)color_error);

    glBindTexture(GL_TEXTURE_2D, depth);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, 64, 64);

    GLenum depth_error = glGetError();
    fprintf(stderr, "[E140] depth-storage error=0x%x\n",
            (unsigned)depth_error);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (color_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, color, 0);
        glDrawBuffers(1, (const GLenum[]){ GL_COLOR_ATTACHMENT0 });
        e140_check("color-only");
    }

    if (depth_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, depth, 0);
        glDrawBuffers(1, (const GLenum[]){ GL_NONE });
        glReadBuffer(GL_NONE);
        e140_check("depth-only");
    }

    if (color_error == GL_NO_ERROR && depth_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, color, 0);
        glDrawBuffers(1, (const GLenum[]){ GL_COLOR_ATTACHMENT0 });
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        e140_check("color+depth");
    }

cleanup:
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);

    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);
    glActiveTexture((GLenum)old_active);

    if (fbo)
        glDeleteFramebuffers(1, &fbo);
    if (color)
        glDeleteTextures(1, &color);
    if (depth)
        glDeleteTextures(1, &depth);

    fprintf(stderr, "[E140] probe finished\n");
    fflush(stderr);
}

/*
 * E153: Synthetic GLES framebuffer format capability test.
 * UI-thread only. No guest pointers or guest GPU commands.
 */
static void e153_check(const char *name)
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GLenum error = glGetError();
    fprintf(stderr,
        "[E153] %s status=0x%x complete=%d error=0x%x\n",
        name, (unsigned)status,
        status == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR,
        (unsigned)error);
}

void gles_e153_framebuffer_probe(void)
{
    GLint old_draw = 0;
    GLint old_read = 0;
    GLint old_active = 0;
    GLint old_texture = 0;

    GLuint color_r8 = 0;
    GLuint color_hdr = 0;
    GLuint depth_stencil = 0;
    GLuint fbo = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);

    glGenTextures(1, &color_r8);
    glGenTextures(1, &color_hdr);
    glGenTextures(1, &depth_stencil);
    glGenFramebuffers(1, &fbo);

    if (!color_r8 || !color_hdr || !depth_stencil || !fbo) {
        fprintf(stderr, "[E153] allocation failed error=0x%x\n",
                (unsigned)glGetError());
        goto cleanup;
    }

    glBindTexture(GL_TEXTURE_2D, color_r8);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 64, 64);
    GLenum r8_error = glGetError();

    glBindTexture(GL_TEXTURE_2D, color_hdr);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R11F_G11F_B10F, 64, 64);
    GLenum hdr_error = glGetError();

    glBindTexture(GL_TEXTURE_2D, depth_stencil);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, 64, 64);
    GLenum ds_error = glGetError();

    fprintf(stderr,
        "[E153] storage R8=0x%x HDR=0x%x D24S8=0x%x\n",
        (unsigned)r8_error, (unsigned)hdr_error,
        (unsigned)ds_error);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (r8_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, color_r8, 0);
        glDrawBuffers(1, (const GLenum[]){GL_COLOR_ATTACHMENT0});
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        e153_check("R8-color-only");
    }

    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, 0, 0);

    if (ds_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, depth_stencil, 0);
        glDrawBuffers(1, (const GLenum[]){GL_NONE});
        glReadBuffer(GL_NONE);
        e153_check("D24S8-only");
    }

    if (hdr_error == GL_NO_ERROR && ds_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, color_hdr, 0);
        glDrawBuffers(1, (const GLenum[]){GL_COLOR_ATTACHMENT0});
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        e153_check("HDR+D24S8");
    }

    if (r8_error == GL_NO_ERROR && ds_error == GL_NO_ERROR) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, color_r8, 0);
        glDrawBuffers(1, (const GLenum[]){GL_COLOR_ATTACHMENT0});
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        e153_check("R8+D24S8");
    }

cleanup:
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);
    glActiveTexture((GLenum)old_active);

    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (color_r8) glDeleteTextures(1, &color_r8);
    if (color_hdr) glDeleteTextures(1, &color_hdr);
    if (depth_stencil) glDeleteTextures(1, &depth_stencil);

    fprintf(stderr, "[E153] probe finished\n");
    fflush(stderr);
}
