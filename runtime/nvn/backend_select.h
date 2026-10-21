#pragma once

typedef enum {
    NVN_BACKEND_VULKAN = 0,
    NVN_BACKEND_GLES_EXPERIMENTAL = 1,
    NVN_BACKEND_VULKAN11_EXPERIMENTAL = 2
} NvnBackendKind;

void nvn_backend_lock(void);
NvnBackendKind nvn_backend_kind(void);
const char *nvn_backend_name(void);