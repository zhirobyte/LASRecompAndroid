#include "texture_manager.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../tegra.h"

#define GLES_NV_RGBA8 0x25u
#define GLES_MAX_TEXTURE_BYTES (64ull * 1024ull * 1024ull)

bool gles_upload_nvn_texture(const NvnTexture *t, GLuint *out_name)
{
    if (!out_name)
        return false;

    *out_name = 0;

    if (!t || !t->pool)
        return false;

    /*
     * First implementation: block-linear RGBA8, 2D,
     * single level, layer, depth slice and sample.
     *
     * NVN target 1 is the ordinary 2D texture target.
     * Flag 0x10 indicates linear storage.
     */
    if (t->target != 1 ||
        t->format != GLES_NV_RGBA8 ||
        (t->flags & 0x10u) != 0 ||
        t->width == 0 || t->height == 0 ||
        t->depth != 1 || t->levels != 1 ||
        t->samples != 1 ||
        t->layout.layers != 1 ||
        t->layout.depth != 1 ||
        t->layout.levels != 1)
        return false;

    const FormatInfo *fmt = nvn_format_info(t->format);

    if (!fmt || fmt->bpb != 4 ||
        fmt->bw != 1 || fmt->bh != 1 ||
        fmt->kind != FMT_COLOR)
        return false;

    if (t->layout.fmt != fmt ||
        t->layout.width != t->width ||
        t->layout.height != t->height)
        return false;

    uint64_t pixels = (uint64_t)t->width * t->height;
    uint64_t linear_size = pixels * 4u;

    if (!pixels ||
        linear_size > GLES_MAX_TEXTURE_BYTES)
        return false;

    uint64_t storage_size = tegra_total_size(&t->layout);

    if (!storage_size ||
        storage_size > GLES_MAX_TEXTURE_BYTES)
        return false;

    const NvnPool *pool = t->pool;

    if (!pool->storage ||
        t->pool_offset > pool->size ||
        storage_size > pool->size - t->pool_offset)
        return false;

    if (pool->storage > UINT64_MAX - t->pool_offset)
        return false;

    uint64_t address = pool->storage + t->pool_offset;

    if (t->addr != address)
        return false;

    LevelLayout level = tegra_level(&t->layout, 0);

    if (level.wb != t->width ||
        level.hb != t->height ||
        level.d != 1 ||
        level.offset > storage_size)
        return false;

    /*
     * These checks validate declared pool bounds.
     * The caller must still ensure pool->storage points
     * to a valid, mapped host-accessible memory region.
     */
    uint8_t *linear = malloc((size_t)linear_size);

    if (!linear)
        return false;

    const uint8_t *src =
        (const uint8_t *)(uintptr_t)address + level.offset;

    tegra_deswizzle(
        src,
        linear,
        level.wb,
        level.hb,
        1,
        fmt->bpb,
        level.block_height_log2,
        level.block_depth_log2
    );

    GLint previous_active = 0;
    GLint previous_texture = 0;
    GLint previous_unpack = 4;
    GLint previous_unpack_row_length = 0;
    GLint previous_unpack_skip_pixels = 0;
    GLint previous_unpack_skip_rows = 0;
    GLint max_texture_size = 0;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

    if (t->width > (uint32_t)max_texture_size ||
        t->height > (uint32_t)max_texture_size) {
        free(linear);
        return false;
    }

    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previous_unpack_row_length);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &previous_unpack_skip_pixels);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &previous_unpack_skip_rows);

    GLuint name = 0;
    glGenTextures(1, &name);

    bool success = false;

    if (name) {
        glBindTexture(GL_TEXTURE_2D, name);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            (GLsizei)t->width,
            (GLsizei)t->height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            linear
        );

        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        success = glGetError() == GL_NO_ERROR;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, previous_unpack_row_length);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, previous_unpack_skip_pixels);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, previous_unpack_skip_rows);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glActiveTexture((GLenum)previous_active);

    free(linear);

    if (!success) {
        if (name)
            glDeleteTextures(1, &name);
        return false;
    }

    *out_name = name;
    return true;
}
