#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GLES_IDENTITY_CAPACITY 128

typedef struct {
    const void *pointer;
    uint64_t generation;
} GlesObjectIdentity;

typedef struct {
    const void *pointer;
    uint64_t generation;
    bool active;
} GlesIdentitySlot;

typedef struct {
    GlesIdentitySlot slots[GLES_IDENTITY_CAPACITY];
    uint64_t next_generation;
} GlesIdentityRegistry;

void gles_identity_init(GlesIdentityRegistry *registry);

bool gles_identity_register(
    GlesIdentityRegistry *registry,
    const void *pointer,
    GlesObjectIdentity *identity
);

bool gles_identity_matches(
    const GlesIdentityRegistry *registry,
    GlesObjectIdentity identity
);

bool gles_identity_retire(
    GlesIdentityRegistry *registry,
    GlesObjectIdentity identity
);
