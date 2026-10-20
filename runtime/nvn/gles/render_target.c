/*
 * Experimental NVN -> GLES render-target registry.
 * Synthetic tests only. Not a gameplay texture manager.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "../nvn.h"

#define GLES_RT_MAX 16

typedef struct {
    uint64_t tex[8];
    NvnTextureView views[8];
    uint8_t has_view[8];
    uint64_t depth;
    NvnTextureView dview;
    uint8_t has_dview;
} GlesRtRecord;

typedef struct {
    const NvnTexture *texture;
    GLuint gl_texture;
    GLuint framebuffer;
} GlesRtEntry;

_Static_assert(sizeof(NvnTextureView) == 28,
               "Unexpected NVN texture-view layout");

_Static_assert(sizeof(GlesRtRecord) % 8 == 0,
               "Render-target record alignment changed");

static GlesRtEntry entries[GLES_RT_MAX];

static GlesRtEntry *find_entry(const NvnTexture *texture)
{
    for (unsigned i = 0; i < GLES_RT_MAX; i++) {
        if (entries[i].texture == texture && texture)
            return &entries[i];
    }
    return NULL;
}

bool gles_register_test_render_target(
    const NvnTexture *texture, unsigned gl_texture)
{
    if (!texture || !gl_texture || find_entry(texture))
        return false;

    if (!texture->width || !texture->height ||
        texture->depth > 1 ||
        texture->levels > 1 ||
        texture->samples > 1)
        return false;

    if (glIsTexture((GLuint)gl_texture) != GL_TRUE)
        return false;

    for (unsigned i = 0; i < GLES_RT_MAX; i++) {
        if (!entries[i].texture) {
            entries[i].texture = texture;
            entries[i].gl_texture = (GLuint)gl_texture;
            entries[i].framebuffer = 0;
            return true;
        }
    }

    return false;
}

void gles_unregister_test_render_target(void)
{
    GLint old_draw = 0;
    GLint old_read = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);

    bool restore_draw = true;
    bool restore_read = true;

    for (unsigned i = 0; i < GLES_RT_MAX; i++) {
        GLuint framebuffer = entries[i].framebuffer;

        if (framebuffer) {
            if ((GLuint)old_draw == framebuffer)
                restore_draw = false;

            if ((GLuint)old_read == framebuffer)
                restore_read = false;

            glDeleteFramebuffers(1, &framebuffer);
        }
    }

    memset(entries, 0, sizeof(entries));

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      restore_draw ? (GLuint)old_draw : 0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      restore_read ? (GLuint)old_read : 0);
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

    if (!rt.tex[0] || rt.has_view[0] ||
        rt.depth || rt.has_dview)
        return false;

    for (unsigned i = 1; i < 8; i++) {
        if (rt.tex[i] || rt.has_view[i])
            return false;
    }

    GlesRtEntry *entry = find_entry(
        (const NvnTexture *)(uintptr_t)rt.tex[0]);

    if (!entry)
        return false;

    GLint old_draw = 0;
    GLint old_read = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);

    if (!entry->framebuffer)
        glGenFramebuffers(1, &entry->framebuffer);

    if (!entry->framebuffer)
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, entry->framebuffer);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        entry->gl_texture,
        0);

    bool ok =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_COMPLETE;

    if (glGetError() != GL_NO_ERROR)
        ok = false;

    if (!ok) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                          (GLuint)old_draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,
                          (GLuint)old_read);
    }

    return ok;
}