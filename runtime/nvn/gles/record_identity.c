#include "record_identity.h"

#include <string.h>

bool gles_record_identity_init(GlesRecordIdentityRegistry *registry)
{
    if (!registry)
        return false;

    memset(registry, 0, sizeof(*registry));

    if (pthread_mutex_init(&registry->mutex, NULL) != 0)
        return false;

    registry->next_generation = 1;
    registry->initialized = true;
    return true;
}

void gles_record_identity_destroy(GlesRecordIdentityRegistry *registry)
{
    if (!registry || !registry->initialized)
        return;

    pthread_mutex_destroy(&registry->mutex);
    registry->initialized = false;
}

bool gles_record_identity_register(
    GlesRecordIdentityRegistry *registry,
    uintptr_t record_address,
    GlesRecordIdentityKey *out)
{
    if (!registry || !registry->initialized ||
        !record_address || !out)
        return false;

    pthread_mutex_lock(&registry->mutex);

    GlesRecordIdentitySlot *free_slot = NULL;

    for (size_t i = 0; i < GLES_RECORD_IDENTITY_CAPACITY; i++) {
        GlesRecordIdentitySlot *slot = &registry->slots[i];

        if (slot->occupied &&
            slot->key.record_address == record_address) {
            pthread_mutex_unlock(&registry->mutex);
            return false;
        }

        if (!slot->occupied && !free_slot)
            free_slot = slot;
    }

    /* E167D3C_GENERATION_GUARD
 * Refuse to issue UINT64_MAX: incrementing it would wrap to zero.
 * A zero generation is always invalid.
 */
if (!free_slot ||
    registry->next_generation == 0 ||
    registry->next_generation == UINT64_MAX) {
        pthread_mutex_unlock(&registry->mutex);
        return false;
    }

    GlesRecordIdentityKey key = {
        .record_address = record_address,
        .generation = registry->next_generation++
    };

    free_slot->key = key;
    free_slot->occupied = true;
    *out = key;

    pthread_mutex_unlock(&registry->mutex);
    return true;
}

bool gles_record_identity_lookup(
    GlesRecordIdentityRegistry *registry,
    GlesRecordIdentityKey key)
{
    if (!registry || !registry->initialized ||
        !key.record_address || !key.generation)
        return false;

    pthread_mutex_lock(&registry->mutex);

    bool found = false;

    for (size_t i = 0; i < GLES_RECORD_IDENTITY_CAPACITY; i++) {
        GlesRecordIdentitySlot *slot = &registry->slots[i];

        if (slot->occupied &&
            slot->key.record_address == key.record_address &&
            slot->key.generation == key.generation) {
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&registry->mutex);
    return found;
}

bool gles_record_identity_retire(
    GlesRecordIdentityRegistry *registry,
    GlesRecordIdentityKey key)
{
    if (!registry || !registry->initialized ||
        !key.record_address || !key.generation)
        return false;

    pthread_mutex_lock(&registry->mutex);

    bool retired = false;

    for (size_t i = 0; i < GLES_RECORD_IDENTITY_CAPACITY; i++) {
        GlesRecordIdentitySlot *slot = &registry->slots[i];

        if (slot->occupied &&
            slot->key.record_address == key.record_address &&
            slot->key.generation == key.generation) {
            memset(slot, 0, sizeof(*slot));
            retired = true;
            break;
        }
    }

    pthread_mutex_unlock(&registry->mutex);
    return retired;
}

size_t gles_record_identity_count(
    GlesRecordIdentityRegistry *registry)
{
    if (!registry || !registry->initialized)
        return 0;

    pthread_mutex_lock(&registry->mutex);

    size_t count = 0;

    for (size_t i = 0; i < GLES_RECORD_IDENTITY_CAPACITY; i++)
        if (registry->slots[i].occupied)
            count++;

    pthread_mutex_unlock(&registry->mutex);
    return count;
}
