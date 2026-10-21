#include "snapshot_registry.h"

#include <stdio.h>
#include <string.h>

static int check(bool condition, const char *name)
{
    fprintf(stderr, "[E157] %s=%s\n",
            name, condition ? "PASS" : "FAIL");
    return condition ? 0 : 1;
}

static GlesTextureSnapshot make_snapshot(uintptr_t pointer, uint64_t id)
{
    GlesTextureSnapshot s = {0};
    s.key.pointer_value = pointer;
    s.key.debug_id = id;
    s.target = 1;
    s.format = 0x3f;
    s.width = 1920;
    s.height = 1080;
    s.depth = 1;
    s.levels = 1;
    return s;
}

int gles_e157_snapshot_registry_selftest(void)
{
    int failures = 0;
    GlesSnapshotRegistry registry;

    if (!gles_snapshot_registry_init(&registry)) {
        fprintf(stderr, "[E157] registry-init=FAIL\n");
        return 0;
    }

    failures += check(true, "registry-init");

    GlesTextureSnapshot original = make_snapshot(0x12340000u, 181);
    GlesTextureSnapshotKey first = {0};
    GlesTextureSnapshotKey second = {0};
    GlesTextureSnapshot out = {0};

    failures += check(
        gles_snapshot_registry_register(&registry, &original, &first),
        "register-first");

    failures += check(
        gles_snapshot_registry_count(&registry) == 1,
        "count-one");

    failures += check(
        !gles_snapshot_registry_register(&registry, &original, &second),
        "duplicate-live-rejected");

    failures += check(
        gles_snapshot_registry_lookup(&registry, first, &out),
        "lookup-first");

    failures += check(
        out.width == 1920 && out.key.generation == first.generation,
        "metadata-copy");

    failures += check(
        gles_snapshot_registry_retire(&registry, first),
        "retire-first");

    failures += check(
        !gles_snapshot_registry_lookup(&registry, first, &out),
        "retired-lookup-rejected");

    GlesTextureSnapshot replacement = make_snapshot(0x12340000u, 182);

    failures += check(
        gles_snapshot_registry_register(&registry, &replacement, &second),
        "pointer-reuse-register");

    failures += check(
        second.generation > first.generation,
        "generation-advanced");

    failures += check(
        !gles_snapshot_registry_lookup(&registry, first, &out),
        "stale-generation-rejected");

    failures += check(
        !gles_snapshot_registry_retire(&registry, first),
        "stale-retirement-rejected");

    failures += check(
        gles_snapshot_registry_lookup(&registry, second, &out) &&
        out.key.debug_id == 182,
        "replacement-lookup");

    failures += check(
        gles_snapshot_registry_retire(&registry, second),
        "retire-replacement");

    failures += check(
        gles_snapshot_registry_count(&registry) == 0,
        "registry-empty");

    /*
     * Capacity test: all distinct live pointers.
     * No real pointer is dereferenced.
     */
    GlesTextureSnapshotKey keys[GLES_SNAPSHOT_REGISTRY_CAPACITY];

    bool filled = true;
    for (size_t i = 0; i < GLES_SNAPSHOT_REGISTRY_CAPACITY; i++) {
        GlesTextureSnapshot s =
            make_snapshot((uintptr_t)(0x10000000u + i * 256u), i + 1);

        if (!gles_snapshot_registry_register(&registry, &s, &keys[i])) {
            filled = false;
            break;
        }
    }

    failures += check(filled, "capacity-fill");

    GlesTextureSnapshot extra = make_snapshot(0x90000000u, 9999);
    GlesTextureSnapshotKey extra_key = {0};

    failures += check(
        !gles_snapshot_registry_register(&registry, &extra, &extra_key),
        "capacity-overflow-rejected");

    for (size_t i = 0; i < GLES_SNAPSHOT_REGISTRY_CAPACITY; i++) {
        if (filled)
            gles_snapshot_registry_retire(&registry, keys[i]);
    }

    failures += check(
        gles_snapshot_registry_count(&registry) == 0,
        "capacity-cleanup");

    gles_snapshot_registry_destroy(&registry);

    fprintf(stderr, "[E157] registry selftest failures=%d\n", failures);
    return failures == 0;
}
