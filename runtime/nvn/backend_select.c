#include "backend_select.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <unistd.h>
#endif

/*
 * Backend selection is published once before game threads start.
 * State: 0 = unlocked, 1 = initializing, 2 = ready.
 */
static atomic_int backend_state = ATOMIC_VAR_INIT(0);
static NvnBackendKind selected_backend = NVN_BACKEND_VULKAN;

static NvnBackendKind detect_backend(void) {
#ifdef __ANDROID__
    const char *value = getenv("LARECOMP_RENDER_BACKEND");

    if (value && strcmp(value, "gles") == 0) {
        return NVN_BACKEND_GLES_EXPERIMENTAL;
    }

    const char *vk11_marker =
        "/sdcard/Android/data/com.lasrecomp.android/files/"
        "vulkan11-backend-experimental";

    if (access(vk11_marker, F_OK) == 0) {
        return NVN_BACKEND_VULKAN11_EXPERIMENTAL;
    }

    const char *marker =
        "/sdcard/Android/data/com.lasrecomp.android/files/"
        "gles-backend-experimental";

    if (access(marker, F_OK) == 0) {
        return NVN_BACKEND_GLES_EXPERIMENTAL;
    }
#endif

    return NVN_BACKEND_VULKAN;
}

void nvn_backend_lock(void) {
    int expected = 0;

    if (atomic_compare_exchange_strong_explicit(
            &backend_state, &expected, 1,
            memory_order_acq_rel, memory_order_acquire)) {
        selected_backend = detect_backend();
        atomic_store_explicit(
            &backend_state, 2, memory_order_release);
        return;
    }

    while (atomic_load_explicit(
            &backend_state, memory_order_acquire) != 2) {
        /* Another thread is publishing the selection. */
    }
}

NvnBackendKind nvn_backend_kind(void) {
    nvn_backend_lock();
    return selected_backend;
}

const char *nvn_backend_name(void) {
    NvnBackendKind kind = nvn_backend_kind();
    if (kind == NVN_BACKEND_VULKAN11_EXPERIMENTAL) return "vulkan11-experimental";
    return kind == NVN_BACKEND_GLES_EXPERIMENTAL ? "gles-experimental" : "vulkan";
}