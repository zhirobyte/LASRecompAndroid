#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

#include "texture_cache.h"
#include "../tegra.h"

static bool read_pixel(GLuint texture, uint8_t pixel[4])
{
    GLint old_draw = 0, old_read = 0;
    GLint old_pack = 4, old_row = 0, old_skip_x = 0, old_skip_y = 0;
    GLuint framebuffer = 0;
    bool ok = false;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &old_row);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &old_skip_x);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &old_skip_y);

    glGenFramebuffers(1, &framebuffer);

    if (framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, texture, 0
        );

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
            GL_FRAMEBUFFER_COMPLETE) {

            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, 0);
            glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_PACK_SKIP_ROWS, 0);

            glReadPixels(0, 0, 1, 1,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixel);

            ok = glGetError() == GL_NO_ERROR;
        }
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)old_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read);

    if (framebuffer)
        glDeleteFramebuffers(1, &framebuffer);

    glPixelStorei(GL_PACK_ALIGNMENT, old_pack);
    glPixelStorei(GL_PACK_ROW_LENGTH, old_row);
    glPixelStorei(GL_PACK_SKIP_PIXELS, old_skip_x);
    glPixelStorei(GL_PACK_SKIP_ROWS, old_skip_y);

    return ok;
}

static void fill_pixels(uint8_t pixels[64],
                        uint8_t red, uint8_t green, uint8_t blue)
{
    for (unsigned i = 0; i < 16; i++) {
        pixels[i * 4 + 0] = red;
        pixels[i * 4 + 1] = green;
        pixels[i * 4 + 2] = blue;
        pixels[i * 4 + 3] = 255;
    }
}

static bool run_test(void)
{
    bool ok = false;
    GlesTextureCache cache;
    gles_texture_cache_init(&cache);

    NvnTexture texture = {0};
    NvnPool pool = {0};

    const FormatInfo *fmt = nvn_format_info(0x25);

    if (!fmt)
        return false;

    texture.target = 1;
    texture.format = 0x25;
    texture.width = 4;
    texture.height = 4;
    texture.depth = 1;
    texture.levels = 1;
    texture.samples = 1;

    texture.layout.width = 4;
    texture.layout.height = 4;
    texture.layout.depth = 1;
    texture.layout.layers = 1;
    texture.layout.levels = 1;
    texture.layout.fmt = fmt;

    tegra_layout_init(&texture.layout);

    uint64_t required = tegra_total_size(&texture.layout);

    if (!required || required > 4096)
        return false;

    const size_t offset = 512;
    uint8_t *memory = calloc(1, offset + (size_t)required);

    if (!memory)
        return false;

    pool.storage = (uint64_t)(uintptr_t)memory;
    pool.size = offset + required;

    texture.pool = &pool;
    texture.pool_offset = offset;
    texture.addr = pool.storage + offset;

    LevelLayout level = tegra_level(&texture.layout, 0);

    uint8_t pixels[64];
    uint8_t actual[4] = {0};

    fill_pixels(pixels, 255, 0, 0);

    tegra_swizzle(
        pixels, memory + offset + level.offset,
        level.wb, level.hb, 1, fmt->bpb,
        level.block_height_log2, level.block_depth_log2
    );

    GLuint first = 0, second = 0, refreshed = 0;

    if (!gles_texture_cache_get(&cache, &texture, &first))
        goto cleanup;

    bool initial =
        first != 0 &&
        cache.uploads == 1 &&
        read_pixel(first, actual) &&
        actual[0] == 255 &&
        actual[1] == 0 &&
        actual[2] == 0 &&
        actual[3] == 255;

    fprintf(stderr, "[GLES-CACHE] INITIAL UPLOAD: %s\n",
            initial ? "PASS" : "FAIL");

    if (!initial)
        goto cleanup;

    if (!gles_texture_cache_get(&cache, &texture, &second))
        goto cleanup;

    bool hit =
        first == second &&
        cache.uploads == 1 &&
        cache.hits == 1;

    fprintf(stderr, "[GLES-CACHE] CACHE HIT: %s\n",
            hit ? "PASS" : "FAIL");

    if (!hit)
        goto cleanup;

    fill_pixels(pixels, 0, 255, 0);

    tegra_swizzle(
        pixels, memory + offset + level.offset,
        level.wb, level.hb, 1, fmt->bpb,
        level.block_height_log2, level.block_depth_log2
    );

    if (!gles_texture_cache_invalidate(&cache, &texture))
        goto cleanup;

    if (!gles_texture_cache_get(&cache, &texture, &refreshed))
        goto cleanup;

    bool refresh =
        refreshed != 0 &&
        cache.uploads == 2 &&
        cache.refreshes == 1 &&
        read_pixel(refreshed, actual) &&
        actual[0] == 0 &&
        actual[1] == 255 &&
        actual[2] == 0 &&
        actual[3] == 255;

    fprintf(stderr, "[GLES-CACHE] INVALIDATE REFRESH: %s\n",
            refresh ? "PASS" : "FAIL");

    if (!refresh)
        goto cleanup;

    /*
     * A failed refresh must preserve the old GPU texture
     * and leave the cache entry dirty.
     */
    if (!gles_texture_cache_invalidate(&cache, &texture))
        goto cleanup;

    uint64_t old_size = pool.size;
    pool.size = offset + required - 1;

    GLuint rejected = 999;
    bool rejected_refresh =
        !gles_texture_cache_get(&cache, &texture, &rejected) &&
        rejected == 0 &&
        cache.uploads == 2 &&
        cache.refreshes == 1 &&
        cache.entries[0].dirty &&
        cache.entries[0].name == refreshed &&
        glIsTexture(refreshed) == GL_TRUE;

    pool.size = old_size;

    fprintf(stderr, "[GLES-CACHE] FAILED REFRESH: %s\n",
            rejected_refresh ? "PASS" : "FAIL");

    if (!rejected_refresh)
        goto cleanup;

    gles_texture_cache_release(&cache, &texture);

    bool released =
        glIsTexture(refreshed) == GL_FALSE &&
        cache.entries[0].texture == NULL;

    fprintf(stderr, "[GLES-CACHE] RELEASE: %s\n",
            released ? "PASS" : "FAIL");

    if (!released)
        goto cleanup;

    GLuint new_name = 0;

    if (!gles_texture_cache_get(&cache, &texture, &new_name))
        goto cleanup;

    gles_texture_cache_reset(&cache);

    bool reset =
        glIsTexture(new_name) == GL_FALSE &&
        cache.uploads == 0 &&
        cache.hits == 0 &&
        cache.refreshes == 0;

    fprintf(stderr, "[GLES-CACHE] RESET: %s\n",
            reset ? "PASS" : "FAIL");

    ok = reset;

cleanup:
    gles_texture_cache_reset(&cache);
    free(memory);
    return ok;
}

static bool run_range_test(void)
{
    GlesTextureCache cache;
    gles_texture_cache_init(&cache);

    /*
     * These are metadata-only synthetic entries.
     * No GL objects are created or deleted.
     */
    NvnTexture a = {0};
    NvnTexture b = {0};
    NvnPool pool = {0};

    const FormatInfo *fmt = nvn_format_info(0x25);

    if (!fmt)
        return false;

    a.width = b.width = 4;
    a.height = b.height = 4;
    a.depth = b.depth = 1;

    a.layout.width = b.layout.width = 4;
    a.layout.height = b.layout.height = 4;
    a.layout.depth = b.layout.depth = 1;
    a.layout.layers = b.layout.layers = 1;
    a.layout.levels = b.layout.levels = 1;
    a.layout.fmt = b.layout.fmt = fmt;

    tegra_layout_init(&a.layout);
    tegra_layout_init(&b.layout);

    uint64_t size = tegra_total_size(&a.layout);

    if (!size || size > 4096)
        return false;

    /*
     * Fake addresses are safe here because the range
     * invalidator only compares addresses.
     */
    pool.storage = 0x100000;
    pool.size = 0x10000;

    a.pool = &pool;
    b.pool = &pool;

    a.addr = pool.storage + 0x1000;
    b.addr = pool.storage + 0x4000;

    cache.entries[0].texture = &a;
    cache.entries[0].pool_identity = &pool;
    cache.entries[0].guest_addr = a.addr;
    cache.entries[0].guest_size = size;

    cache.entries[1].texture = &b;
    cache.entries[1].pool_identity = &pool;
    cache.entries[1].guest_addr = b.addr;
    cache.entries[1].guest_size = size;

    size_t count = gles_texture_cache_invalidate_range(
        &cache, a.addr, 1
    );

    bool direct =
        count == 1 &&
        cache.entries[0].dirty &&
        !cache.entries[1].dirty;

    fprintf(stderr, "[GLES-CACHE-RANGE] DIRECT: %s\n",
            direct ? "PASS" : "FAIL");

    cache.entries[0].dirty = false;

    count = gles_texture_cache_invalidate_range(
        &cache, a.addr + size, 1
    );

    bool boundary =
        count == 0 &&
        !cache.entries[0].dirty &&
        !cache.entries[1].dirty;

    fprintf(stderr, "[GLES-CACHE-RANGE] BOUNDARY: %s\n",
            boundary ? "PASS" : "FAIL");

    count = gles_texture_cache_invalidate_range(
        &cache, a.addr - 1, 1
    );

    bool before =
        count == 0 &&
        !cache.entries[0].dirty;

    fprintf(stderr, "[GLES-CACHE-RANGE] BEFORE: %s\n",
            before ? "PASS" : "FAIL");

    count = gles_texture_cache_invalidate_range(
        &cache, a.addr - 1, 2
    );

    bool overlap =
        count == 1 &&
        cache.entries[0].dirty &&
        !cache.entries[1].dirty;

    fprintf(stderr, "[GLES-CACHE-RANGE] OVERLAP: %s\n",
            overlap ? "PASS" : "FAIL");

    cache.entries[0].dirty = false;

    count = gles_texture_cache_invalidate_range(
        &cache, a.addr, 0
    );

    bool empty =
        count == 0 &&
        !cache.entries[0].dirty;

    fprintf(stderr, "[GLES-CACHE-RANGE] EMPTY: %s\n",
            empty ? "PASS" : "FAIL");

    count = gles_texture_cache_invalidate_range(
        &cache, a.addr, b.addr - a.addr + 1
    );

    bool both =
        count == 2 &&
        cache.entries[0].dirty &&
        cache.entries[1].dirty;

    fprintf(stderr, "[GLES-CACHE-RANGE] BOTH: %s\n",
            both ? "PASS" : "FAIL");

    /*
     * Synthetic entries have GL name zero, so this
     * exercises identity retirement without GL allocation.
     */
    size_t released = gles_texture_cache_release_pool(
        &cache, &pool
    );

    bool retired =
        released == 2 &&
        cache.entries[0].texture == NULL &&
        cache.entries[1].texture == NULL &&
        cache.entries[0].pool_identity == NULL &&
        cache.entries[1].pool_identity == NULL;

    fprintf(stderr, "[GLES-CACHE-RANGE] POOL RETIRE: %s\n",
            retired ? "PASS" : "FAIL");

    bool retired_again =
        gles_texture_cache_release_pool(&cache, &pool) == 0;

    fprintf(stderr, "[GLES-CACHE-RANGE] POOL RETIRE AGAIN: %s\n",
            retired_again ? "PASS" : "FAIL");

    return direct && boundary && before &&
           overlap && empty && both &&
           retired && retired_again;
}

static bool run_identity_retirement_test(void)
{
    GlesTextureCache cache;
    GlesIdentityRegistry registry;

    gles_texture_cache_init(&cache);
    gles_identity_init(&registry);

    NvnTexture texture = {0};

    GlesObjectIdentity old_id = {0};
    GlesObjectIdentity new_id = {0};

    if (!gles_identity_register(&registry, &texture, &old_id))
        return false;

    cache.entries[0].texture = &texture;
    cache.entries[0].identity = old_id;

    bool old_released =
        gles_texture_cache_release_identity(&cache, old_id) &&
        cache.entries[0].texture == NULL;

    bool retired = gles_identity_retire(&registry, old_id);

    bool reused =
        gles_identity_register(&registry, &texture, &new_id);

    cache.entries[0].texture = &texture;
    cache.entries[0].identity = new_id;

    bool stale_rejected =
        !gles_texture_cache_release_identity(&cache, old_id) &&
        cache.entries[0].texture == &texture &&
        cache.entries[0].identity.generation == new_id.generation;

    bool new_released =
        gles_texture_cache_release_identity(&cache, new_id) &&
        cache.entries[0].texture == NULL;

    bool ok =
        old_released && retired && reused &&
        stale_rejected && new_released;

    fprintf(stderr,
            "[GLES-CACHE-IDENTITY] ABA RETIREMENT: %s\n",
            ok ? "PASS" : "FAIL");

    return ok;
}
void gles_run_texture_cache_selftest(void)
{
    bool lifecycle = run_test();
    bool ranges = run_range_test();
    bool identities = run_identity_retirement_test();

    fprintf(stderr, "[GLES-CACHE-RANGE] RESULT: %s\n",
            ranges ? "PASS" : "FAIL");

    fprintf(stderr, "[GLES-CACHE] RESULT: %s\n",
            lifecycle && ranges && identities ? "PASS" : "FAIL");
}
