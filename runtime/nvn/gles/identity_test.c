#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "identity.h"

static bool run_tests(void)
{
    GlesIdentityRegistry registry;
    gles_identity_init(&registry);

    int object_a = 0;
    int object_b = 0;

    GlesObjectIdentity first = {0};
    GlesObjectIdentity second = {0};
    GlesObjectIdentity reused = {0};

    bool registered =
        gles_identity_register(&registry, &object_a, &first) &&
        gles_identity_register(&registry, &object_b, &second);

    bool unique =
        registered &&
        first.generation != second.generation &&
        gles_identity_matches(&registry, first) &&
        gles_identity_matches(&registry, second);

    fprintf(stderr,
            "[GLES-IDENTITY] UNIQUE: %s\n",
            unique ? "PASS" : "FAIL");

    bool duplicate_rejected =
        !gles_identity_register(&registry, &object_a, &reused);

    fprintf(stderr,
            "[GLES-IDENTITY] DUPLICATE: %s\n",
            duplicate_rejected ? "PASS" : "FAIL");

    bool retired =
        gles_identity_retire(&registry, first) &&
        !gles_identity_matches(&registry, first);

    fprintf(stderr,
            "[GLES-IDENTITY] RETIRE: %s\n",
            retired ? "PASS" : "FAIL");

    /*
     * Re-register the same pointer, simulating
     * allocator reuse of the original address.
     */
    bool registered_again =
        gles_identity_register(&registry, &object_a, &reused);

    bool aba_safe =
        registered_again &&
        reused.pointer == first.pointer &&
        reused.generation != first.generation &&
        !gles_identity_matches(&registry, first) &&
        gles_identity_matches(&registry, reused) &&
        !gles_identity_retire(&registry, first) &&
        gles_identity_matches(&registry, reused);

    fprintf(stderr,
            "[GLES-IDENTITY] ABA REUSE: %s\n",
            aba_safe ? "PASS" : "FAIL");

    bool cleanup =
        gles_identity_retire(&registry, second) &&
        gles_identity_retire(&registry, reused);

    fprintf(stderr,
            "[GLES-IDENTITY] CLEANUP: %s\n",
            cleanup ? "PASS" : "FAIL");

    return unique && duplicate_rejected &&
           retired && aba_safe && cleanup;
}

void gles_run_identity_selftest(void)
{
    fprintf(stderr,
            "[GLES-IDENTITY] RESULT: %s\n",
            run_tests() ? "PASS" : "FAIL");
}
