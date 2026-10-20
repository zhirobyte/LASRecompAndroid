#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

#include "texture_manager.h"
#include "../tegra.h"

static bool run_test(void)
{
    bool ok = false;

    const FormatInfo *fmt = nvn_format_info(0x25);

    if (!fmt)
        return false;

    NvnTexture t = {0};
    NvnPool pool = {0};

    t.target = 1;
    t.format = 0x25;
    t.width = 4;
    t.height = 4;
    t.depth = 1;
    t.levels = 1;
    t.samples = 1;

    t.layout.width = 4;
    t.layout.height = 4;
    t.layout.depth = 1;
    t.layout.layers = 1;
    t.layout.levels = 1;
    t.layout.fmt = fmt;

    tegra_layout_init(&t.layout);

    uint64_t required = tegra_total_size(&t.layout);

    if (!required || required > 4096)
        return false;

    const size_t offset = 512;
    const size_t allocation = offset + (size_t)required;

    uint8_t *memory = calloc(1, allocation);

    if (!memory)
        return false;

    pool.storage = (uint64_t)(uintptr_t)memory;
    pool.size = allocation;

    t.pool = &pool;
    t.pool_offset = offset;
    t.addr = pool.storage + offset;

    uint8_t expected[64];

    for (unsigned y = 0; y < 4; y++) {
        for (unsigned x = 0; x < 4; x++) {
            unsigned i = (y * 4 + x) * 4;

            expected[i + 0] = (uint8_t)(20 + x * 40);
            expected[i + 1] = (uint8_t)(30 + y * 50);
            expected[i + 2] = (uint8_t)(70 + x * 10 + y * 5);
            expected[i + 3] = 255;
        }
    }

    LevelLayout level = tegra_level(&t.layout, 0);

    tegra_swizzle(
        expected,
        memory + offset + level.offset,
        level.wb,
        level.hb,
        1,
        fmt->bpb,
        level.block_height_log2,
        level.block_depth_log2
    );

    GLuint invalid_name = 123;

    uint64_t original_size = pool.size;
    pool.size = offset + required - 1;

    bool bounds_rejected =
        !gles_upload_nvn_texture(&t, &invalid_name) &&
        invalid_name == 0;

    pool.size = original_size;

    fprintf(stderr, "[GLES-NVN-TEX] BOUNDS: %s\n",
            bounds_rejected ? "PASS" : "FAIL");

    if (!bounds_rejected)
        goto cleanup;

    t.format = 0x26;

    bool format_rejected =
        !gles_upload_nvn_texture(&t, &invalid_name) &&
        invalid_name == 0;

    t.format = 0x25;

    fprintf(stderr, "[GLES-NVN-TEX] FORMAT: %s\n",
            format_rejected ? "PASS" : "FAIL");

    if (!format_rejected)
        goto cleanup;

    t.flags = 0x10;

    bool linear_rejected =
        !gles_upload_nvn_texture(&t, &invalid_name) &&
        invalid_name == 0;

    t.flags = 0;

    fprintf(stderr, "[GLES-NVN-TEX] LINEAR: %s\n",
            linear_rejected ? "PASS" : "FAIL");

    if (!linear_rejected)
        goto cleanup;

    GLint old_draw = 0;
    GLint old_read = 0;
    GLint old_pack = 4;
    GLint old_pack_row_length = 0;
    GLint old_pack_skip_pixels = 0;
    GLint old_pack_skip_rows = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &old_pack_row_length);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &old_pack_skip_pixels);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &old_pack_skip_rows);

    GLuint texture = 0;
    GLuint framebuffer = 0;

    bool uploaded = gles_upload_nvn_texture(&t, &texture);

    if (uploaded) {
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            texture,
            0
        );

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
            GL_FRAMEBUFFER_COMPLETE) {

            uint8_t actual[64] = {0};

            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, 0);
            glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_PACK_SKIP_ROWS, 0);

            glReadPixels(0, 0, 4, 4,
                         GL_RGBA, GL_UNSIGNED_BYTE, actual);

            ok = glGetError() == GL_NO_ERROR &&
                 memcmp(expected, actual, sizeof(expected)) == 0;
        }
    }

    fprintf(stderr, "[GLES-NVN-TEX] GPU READBACK: %s\n",
            ok ? "PASS" : "FAIL");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);

    if (framebuffer)
        glDeleteFramebuffers(1, &framebuffer);

    if (texture)
        glDeleteTextures(1, &texture);

    glPixelStorei(GL_PACK_ALIGNMENT, old_pack);
    glPixelStorei(GL_PACK_ROW_LENGTH, old_pack_row_length);
    glPixelStorei(GL_PACK_SKIP_PIXELS, old_pack_skip_pixels);
    glPixelStorei(GL_PACK_SKIP_ROWS, old_pack_skip_rows);

cleanup:
    free(memory);
    return ok;
}

void gles_run_nvn_texture_selftest(void)
{
    fprintf(stderr, "[GLES-NVN-TEX] RESULT: %s\n",
            run_test() ? "PASS" : "FAIL");
}
