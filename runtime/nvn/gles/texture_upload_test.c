/*
 * Phase 7C: Tegra block-linear RGBA8 -> GLES texture upload.
 * Diagnostic only; not connected to gameplay.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

#include "../nvn.h"
#include "../tegra.h"

#define TEST_WIDTH 4
#define TEST_HEIGHT 4
#define TEST_FORMAT 0x25

static bool run_upload_test(void)
{
    bool pass = false;
    uint8_t *storage = NULL;
    uint8_t *linear = NULL;

    GLuint texture = 0;
    GLuint framebuffer = 0;

    GLint old_draw = 0;
    GLint old_read = 0;
    GLint old_texture = 0;
    GLint old_unpack = 4;
    GLint old_pack = 4;
    GLint old_active_texture = 0;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_unpack);
    glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack);

    const FormatInfo *format = nvn_format_info(TEST_FORMAT);

    if (!format || format->bpb != 4 ||
        format->bw != 1 || format->bh != 1)
        goto cleanup;

    TexLayout layout = {0};

    layout.width = TEST_WIDTH;
    layout.height = TEST_HEIGHT;
    layout.depth = 1;
    layout.layers = 1;
    layout.levels = 1;
    layout.fmt = format;

    tegra_layout_init(&layout);

    LevelLayout level = tegra_level(&layout, 0);
    uint64_t storage_size = tegra_total_size(&layout);

    if (level.wb != TEST_WIDTH ||
        level.hb != TEST_HEIGHT ||
        storage_size == 0 ||
        storage_size > 1024 * 1024)
        goto cleanup;

    storage = calloc(1, (size_t)storage_size);
    linear = malloc(TEST_WIDTH * TEST_HEIGHT * 4);

    if (!storage || !linear)
        goto cleanup;

    uint8_t expected[TEST_WIDTH * TEST_HEIGHT * 4];

    for (unsigned y = 0; y < TEST_HEIGHT; y++) {
        for (unsigned x = 0; x < TEST_WIDTH; x++) {
            unsigned i = (y * TEST_WIDTH + x) * 4;

            expected[i + 0] = (uint8_t)(20 + x * 40);
            expected[i + 1] = (uint8_t)(30 + y * 50);
            expected[i + 2] = (uint8_t)(70 + x * 10 + y * 5);
            expected[i + 3] = 255;
        }
    }

    /*
     * Emulate guest texture memory:
     * linear source -> Tegra block-linear storage.
     */
    tegra_swizzle(
        expected,
        storage + level.offset,
        level.wb,
        level.hb,
        1,
        format->bpb,
        level.block_height_log2,
        level.block_depth_log2
    );

    /*
     * Read the texture back using the same conversion
     * needed by a GLES upload backend.
     */
    tegra_deswizzle(
        storage + level.offset,
        linear,
        level.wb,
        level.hb,
        1,
        format->bpb,
        level.block_height_log2,
        level.block_depth_log2
    );

    if (memcmp(expected, linear, sizeof(expected)) != 0) {
        fprintf(stderr,
                "[GLES-TEX] Tegra conversion mismatch\n");
        goto cleanup;
    }

    fprintf(stderr,
            "[GLES-TEX] TEGRA ROUNDTRIP: PASS\n");

    glGenTextures(1, &texture);

    if (!texture)
        goto cleanup;

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        TEST_WIDTH,
        TEST_HEIGHT,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        linear
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST
    );

    glGenFramebuffers(1, &framebuffer);

    if (!framebuffer)
        goto cleanup;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        texture,
        0
    );

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
        goto cleanup;

    uint8_t actual[sizeof(expected)];
    memset(actual, 0, sizeof(actual));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(
        0, 0,
        TEST_WIDTH,
        TEST_HEIGHT,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        actual
    );

    if (glGetError() != GL_NO_ERROR)
        goto cleanup;

    if (memcmp(expected, actual, sizeof(expected)) != 0) {
        for (unsigned i = 0; i < sizeof(expected); i++) {
            if (expected[i] != actual[i]) {
                fprintf(stderr,
                    "[GLES-TEX] MISMATCH byte %u: expected %u actual %u\n",
                    i, expected[i], actual[i]);
                break;
            }
        }
        goto cleanup;
    }

    fprintf(stderr,
        "[GLES-TEX] GPU PIXEL READBACK: PASS\n");

    pass = true;

cleanup:
    /*
     * Restore bindings before deleting diagnostic objects.
     */
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);

    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);

    if (framebuffer)
        glDeleteFramebuffers(1, &framebuffer);

    if (texture)
        glDeleteTextures(1, &texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, old_unpack);
    glPixelStorei(GL_PACK_ALIGNMENT, old_pack);
    glActiveTexture((GLenum)old_active_texture);

    free(storage);
    free(linear);

    return pass;
}

void gles_run_texture_upload_selftest(void)
{
    bool pass = run_upload_test();

    fprintf(stderr,
        "[GLES-TEX] RESULT: %s\n",
        pass ? "PASS" : "FAIL");
}