/*
 * Experimental NVN -> GLES render-target mapping.
 * Synthetic tests only. Not a gameplay texture manager.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "../nvn.h"

typedef struct {
    uint64_t tex[8];
    NvnTextureView views[8];
    uint8_t has_view[8];
    uint64_t depth;
    NvnTextureView dview;
    uint8_t has_dview;
} GlesRtRecord;

_Static_assert(sizeof(NvnTextureView) == 28,
               "Unexpected NVN texture-view layout");

_Static_assert(sizeof(GlesRtRecord) % 8 == 0,
               "Render-target record must be aligned");

static struct {
    const NvnTexture *texture;
    GLuint gl_texture;
    GLuint framebuffer;
} registered;

bool gles_register_test_render_target(
    const NvnTexture *texture,
    unsigned gl_texture)
{
    if (!texture || !gl_texture || registered.texture)
        return false;

    if (texture->width == 0 || texture->height == 0)
        return false;

    if (texture->depth > 1 ||
        texture->levels > 1 ||
        texture->samples > 1)
        return false;

    registered.texture = texture;
    registered.gl_texture = (GLuint)gl_texture;
    return true;
}

void gles_unregister_test_render_target(void)
{
    if (registered.framebuffer)
        glDeleteFramebuffers(1, &registered.framebuffer);

    memset(&registered, 0, sizeof(registered));
}

bool gles_execute_set_render_targets(const CmdRec *r)
{
    if (!r || r->op != CMD_SET_RENDER_TARGETS)
        return false;

    if (r->size < sizeof(CmdRec) + sizeof(GlesRtRecord))
        return false;

    if (r->x[0] != 1)
        return false;

    GlesRtRecord rt;
    memcpy(&rt, r->extra, sizeof(rt));

    if (!registered.texture ||
        rt.tex[0] != (uint64_t)(uintptr_t)registered.texture ||
        rt.has_view[0] ||
        rt.depth ||
        rt.has_dview)
        return false;

    for (unsigned i = 1; i < 8; i++) {
        if (rt.tex[i] || rt.has_view[i])
            return false;
    }

    if (!registered.framebuffer)
        glGenFramebuffers(1, &registered.framebuffer);

    if (!registered.framebuffer)
        return false;

    GLint previous_draw = 0;
    GLint previous_read = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);

    glBindFramebuffer(GL_FRAMEBUFFER, registered.framebuffer);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        registered.gl_texture,
        0);

    bool ok =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_COMPLETE;

    if (glGetError() != GL_NO_ERROR)
        ok = false;

    if (!ok) {
        glBindFramebuffer(
            GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw);
        glBindFramebuffer(
            GL_READ_FRAMEBUFFER, (GLuint)previous_read);
    }

    return ok;
}