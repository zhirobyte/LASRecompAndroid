#include "texture_cache.h"
#include "texture_manager.h"

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
