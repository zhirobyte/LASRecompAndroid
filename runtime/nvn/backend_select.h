#pragma once

typedef enum {
    NVN_BACKEND_VULKAN = 0,
    NVN_BACKEND_GLES_EXPERIMENTAL = 1
} NvnBackendKind;

NvnBackendKind nvn_backend_kind(void);
const char *nvn_backend_name(void);