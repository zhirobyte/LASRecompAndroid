#include "texture_snapshot.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char *name)
{
    fprintf(stderr, "[E156] %s=%s\n",
            name, condition ? "PASS" : "FAIL");
    return condition ? 0 : 1;
}

int gles_e156_texture_snapshot_selftest(void)
{
    int failures = 0;

    GlesTextureSnapshot original = {0};
    original.key.pointer_value = (uintptr_t)0x12340000u;
    original.key.generation = 7;
    original.key.debug_id = 181;
    original.target = 1;
    original.format = 0x3f;
    original.width = 1920;
    original.height = 1080;
    original.depth = 1;
    original.levels = 1;

    GlesTextureSnapshot copy = {0};

    failures += check(
        gles_texture_snapshot_valid(&original),
        "valid-metadata");

    failures += check(
        gles_texture_snapshot_copy(&copy, &original),
        "copy-success");

    failures += check(
        memcmp(&copy, &original, sizeof(copy)) == 0,
        "copy-equality");

    failures += check(
        gles_texture_snapshot_matches(&copy, original.key),
        "identity-match");

    GlesTextureSnapshotKey stale = original.key;
    stale.generation--;

    failures += check(
        !gles_texture_snapshot_matches(&copy, stale),
        "stale-generation-rejected");

    GlesTextureSnapshotKey replacement = original.key;
    replacement.debug_id++;

    failures += check(
        !gles_texture_snapshot_matches(&copy, replacement),
        "replacement-rejected");

    original.width = 0;

    failures += check(
        copy.width == 1920,
        "snapshot-independent-of-source");

    failures += check(
        !gles_texture_snapshot_valid(&original),
        "invalid-dimensions-rejected");

    failures += check(
        !gles_texture_snapshot_copy(&copy, &original),
        "invalid-copy-rejected");

    fprintf(stderr, "[E156] snapshot selftest failures=%d\n",
            failures);

    return failures == 0;
}
