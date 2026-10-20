#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <GLES3/gl3.h>

#include "../nvn.h"

#define GLES_TEXTURE_CACHE_CAPACITY 32

typedef struct {
    const NvnTexture *texture;
    GLuint name;
    bool dirty;
} GlesTextureCacheEntry;

typedef struct {
    GlesTextureCacheEntry entries[GLES_TEXTURE_CACHE_CAPACITY];
    uint32_t uploads;
    uint32_t hits;
    uint32_t refreshes;
} GlesTextureCache;

void gles_texture_cache_init(GlesTextureCache *cache);

bool gles_texture_cache_get(
    GlesTextureCache *cache,
    const NvnTexture *texture,
    GLuint *out_name
);

bool gles_texture_cache_invalidate(
    GlesTextureCache *cache,
    const NvnTexture *texture
);

bool gles_texture_cache_refresh(
    GlesTextureCache *cache,
    const NvnTexture *texture
);

void gles_texture_cache_release(
    GlesTextureCache *cache,
    const NvnTexture *texture
);

void gles_texture_cache_reset(GlesTextureCache *cache);

/* Mark textures overlapping [addr, addr + size) as dirty. */
size_t gles_texture_cache_invalidate_range(
    GlesTextureCache *cache,
    uint64_t addr,
    uint64_t size
);
