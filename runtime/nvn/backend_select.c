#include "backend_select.h"

#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <unistd.h>
#endif

NvnBackendKind nvn_backend_kind(void) {
#ifdef __ANDROID__
    const char *value = getenv("LARECOMP_RENDER_BACKEND");

    if (value && strcmp(value, "gles") == 0) {
        return NVN_BACKEND_GLES_EXPERIMENTAL;
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

const char *nvn_backend_name(void) {
    return nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL
        ? "gles-experimental"
        : "vulkan";
}