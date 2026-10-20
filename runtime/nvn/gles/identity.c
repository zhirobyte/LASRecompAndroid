#include "identity.h"

#include <string.h>

void gles_identity_init(GlesIdentityRegistry *registry)
{
    if (registry)
        memset(registry, 0, sizeof(*registry));
}

bool gles_identity_register(
    GlesIdentityRegistry *registry,
    const void *pointer,
    GlesObjectIdentity *identity
) {
    if (!registry || !pointer || !identity)
        return false;

    /*
     * Reject duplicate live registrations.
     */
    for (size_t i = 0; i < GLES_IDENTITY_CAPACITY; i++) {
        GlesIdentitySlot *slot = &registry->slots[i];

        if (slot->active && slot->pointer == pointer)
            return false;
    }

    if (registry->next_generation == UINT64_MAX)
        return false;

    for (size_t i = 0; i < GLES_IDENTITY_CAPACITY; i++) {
        GlesIdentitySlot *slot = &registry->slots[i];

        if (slot->active)
            continue;

        uint64_t generation = ++registry->next_generation;

        slot->pointer = pointer;
        slot->generation = generation;
        slot->active = true;

        identity->pointer = pointer;
        identity->generation = generation;

        return true;
    }

    return false;
}

bool gles_identity_matches(
    const GlesIdentityRegistry *registry,
    GlesObjectIdentity identity
) {
    if (!registry || !identity.pointer || !identity.generation)
        return false;

    for (size_t i = 0; i < GLES_IDENTITY_CAPACITY; i++) {
        const GlesIdentitySlot *slot = &registry->slots[i];

        if (slot->active &&
            slot->pointer == identity.pointer &&
            slot->generation == identity.generation)
            return true;
    }

    return false;
}

bool gles_identity_retire(
    GlesIdentityRegistry *registry,
    GlesObjectIdentity identity
) {
    if (!registry || !identity.pointer || !identity.generation)
        return false;

    for (size_t i = 0; i < GLES_IDENTITY_CAPACITY; i++) {
        GlesIdentitySlot *slot = &registry->slots[i];

        if (slot->active &&
            slot->pointer == identity.pointer &&
            slot->generation == identity.generation) {
            slot->active = false;
            slot->pointer = NULL;
            return true;
        }
    }

    return false;
}
