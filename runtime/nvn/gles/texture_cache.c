#include "texture_cache.h"
#include "texture_manager.h"
#include "../tegra.h"

#include <string.h>

static GlesTextureCacheEntry *find_entry(
    GlesTextureCache *cache,
    const NvnTexture *texture
) {
    for (size_t i = 0; i < GLES_TEXTURE_CACHE_CAPACITY; i++) {
        if (cache->entries[i].texture == texture)
            return &cache->entries[i];
    }

    return NULL;
}

static GlesTextureCacheEntry *free_entry(
    GlesTextureCache *cache
) {
    for (size_t i = 0; i < GLES_TEXTURE_CACHE_CAPACITY; i++) {
        if (!cache->entries[i].texture)
            return &cache->entries[i];
    }

    return NULL;
}

void gles_texture_cache_init(GlesTextureCache *cache)
{
    if (!cache)
        return;

    memset(cache, 0, sizeof(*cache));
}

bool gles_texture_cache_get(
    GlesTextureCache *cache,
    const NvnTexture *texture,
    GLuint *out_name
) {
    if (out_name)
        *out_name = 0;

    if (!cache || !texture || !out_name)
        return false;

    GlesTextureCacheEntry *entry = find_entry(cache, texture);

    if (entry && !entry->dirty) {
        cache->hits++;
        *out_name = entry->name;
        return true;
    }

    if (entry && entry->dirty) {
        if (!gles_texture_cache_refresh(cache, texture))
            return false;

        *out_name = entry->name;
        return true;
    }

    entry = free_entry(cache);

    if (!entry)
        return false;

    GLuint name = 0;

    if (!gles_upload_nvn_texture(texture, &name))
        return false;

    entry->texture = texture;
    entry->pool_identity = texture->pool;
    entry->guest_addr = texture->addr;
    entry->guest_size = tegra_total_size(&texture->layout);
    entry->name = name;
    entry->dirty = false;

    cache->uploads++;
    *out_name = name;

    return true;
}

bool gles_texture_cache_invalidate(
    GlesTextureCache *cache,
    const NvnTexture *texture
) {
    if (!cache || !texture)
        return false;

    GlesTextureCacheEntry *entry = find_entry(cache, texture);

    if (!entry)
        return false;

    entry->dirty = true;
    return true;
}

bool gles_texture_cache_refresh(
    GlesTextureCache *cache,
    const NvnTexture *texture
) {
    if (!cache || !texture)
        return false;

    GlesTextureCacheEntry *entry = find_entry(cache, texture);

    if (!entry)
        return false;

    /*
     * Upload a replacement first. If the upload fails,
     * retain the old texture and keep the entry dirty.
     */
    GLuint replacement = 0;

    if (!gles_upload_nvn_texture(texture, &replacement))
        return false;

    GLuint old_name = entry->name;

    entry->name = replacement;
    entry->pool_identity = texture->pool;
    entry->guest_addr = texture->addr;
    entry->guest_size = tegra_total_size(&texture->layout);
    entry->dirty = false;

    if (old_name)
        glDeleteTextures(1, &old_name);

    cache->uploads++;
    cache->refreshes++;

    return true;
}

void gles_texture_cache_release(
    GlesTextureCache *cache,
    const NvnTexture *texture
) {
    if (!cache || !texture)
        return;

    GlesTextureCacheEntry *entry = find_entry(cache, texture);

    if (!entry)
        return;

    if (entry->name)
        glDeleteTextures(1, &entry->name);

    memset(entry, 0, sizeof(*entry));
}

void gles_texture_cache_reset(GlesTextureCache *cache)
{
    if (!cache)
        return;

    for (size_t i = 0; i < GLES_TEXTURE_CACHE_CAPACITY; i++) {
        GlesTextureCacheEntry *entry = &cache->entries[i];

        if (entry->name)
            glDeleteTextures(1, &entry->name);
    }

    memset(cache, 0, sizeof(*cache));
}

/*
 * Range invalidation only updates cache metadata.
 * No GL context is needed for this operation.
 *
 * Caller must serialize access to the cache when
 * multiple threads can use it.
 */
size_t gles_texture_cache_invalidate_range(
    GlesTextureCache *cache,
    uint64_t addr,
    uint64_t size
) {
    if (!cache || !size)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < GLES_TEXTURE_CACHE_CAPACITY; i++) {
        GlesTextureCacheEntry *entry = &cache->entries[i];
        if (!entry->texture || !entry->pool_identity ||
            !entry->guest_addr || !entry->guest_size)
            continue;

        uint64_t texture_size = entry->guest_size;
        uint64_t texture_addr = entry->guest_addr;

        /*
         * Half-open ranges:
         * [addr, addr + size)
         * [t->addr, t->addr + texture_size)
         *
         * Subtraction avoids integer overflow.
         */
        bool overlap;

        if (addr >= texture_addr)
            overlap = addr - texture_addr < texture_size;
        else
            overlap = texture_addr - addr < size;

        if (overlap) {
            entry->dirty = true;
            count++;
        }
    }

    return count;
}

/*
 * Retire all cached textures belonging to a pool.
 * Compares saved pointer identities only; never dereferences
 * the potentially finalized pool or texture.
 *
 * Requires the owning GLES context to be current.
 */
size_t gles_texture_cache_release_pool(
    GlesTextureCache *cache,
    const NvnPool *pool
) {
    if (!cache || !pool)
        return 0;

    size_t released = 0;

    for (size_t i = 0; i < GLES_TEXTURE_CACHE_CAPACITY; i++) {
        GlesTextureCacheEntry *entry = &cache->entries[i];

        if (!entry->texture || entry->pool_identity != pool)
            continue;

        if (entry->name)
            glDeleteTextures(1, &entry->name);

        memset(entry, 0, sizeof(*entry));
        released++;
    }

    return released;
}