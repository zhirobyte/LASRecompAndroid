// Vulkan device, window, swapchain and presentation (counterpart of the window/present half of metal.m).
#include "../backend_select.h"
#include "../gles/submission_queue.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#endif
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../paths.h"
#include "../../settings.h"
#include "../../ui/menu.h"
#include "vk.h"
#include "../output_scale.h"
#include "../renderer.h"

typedef struct {
    uint32_t depth_enable;
    uint32_t stencil_mask;
    uint32_t stencil_value;
    float depth_value;
} E162DepthClear;

extern bool e162_get_depth_clear(E162DepthClear *out);


VkG g_vk;
static SDL_Window *window;
#ifdef __ANDROID__
// Owned by the UI thread. GLES resource teardown is not yet integrated.
static SDL_GLContext g_gles_context;
#endif
static atomic_bool ui_ready;
static VkDebugUtilsMessengerEXT messenger;

// ================================================================== instance / device
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagsEXT type,
                                               const VkDebugUtilsMessengerCallbackDataEXT *d, void *user) {
    (void)type, (void)user;
    static int n;
    if (sev >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT && n++ < 500) LOG("[vk] %s\n", d->pMessage);
    return VK_FALSE;
}

// LARECOMP_VK_VALIDATION=1: Khronos validation layer (MSYS2 package vulkan-validation-layers)
static void create_instance(void) {
#ifdef __ANDROID__
    vk_android_load();
#endif
    Uint32 n_sdl = 0;
    const char *const *sdl_ext = SDL_Vulkan_GetInstanceExtensions(&n_sdl);
    const char *ext[16];
    uint32_t n_ext = 0;
    for (Uint32 i = 0; i < n_sdl && n_ext < 14; i++) ext[n_ext++] = sdl_ext[i];
    bool validate = getenv("LARECOMP_VK_VALIDATION") != NULL;
    if (validate) ext[n_ext++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "LARecomp",
                             .pEngineName = "LARecomp", .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
                               .enabledExtensionCount = n_ext, .ppEnabledExtensionNames = ext,
                               .enabledLayerCount = validate ? 1 : 0, .ppEnabledLayerNames = layers};
    VkResult r = vkCreateInstance(&ci, NULL, &g_vk.inst);
    if (r != VK_SUCCESS && validate) {
        LOG("[vk] validation layer unavailable (%d); continuing without\n", r);
        ci.enabledLayerCount = 0;
        ci.enabledExtensionCount--;
        validate = false;
        r = vkCreateInstance(&ci, NULL, &g_vk.inst);
    }
    if (r != VK_SUCCESS) FATAL("vkCreateInstance: %d (is a Vulkan 1.3 driver installed?)\n", r);
#ifdef __ANDROID__
    volkLoadInstance(g_vk.inst);
#endif
    if (validate) {
        PFN_vkCreateDebugUtilsMessengerEXT f =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_vk.inst, "vkCreateDebugUtilsMessengerEXT");
        VkDebugUtilsMessengerCreateInfoEXT mi = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_cb};
        if (f) f(g_vk.inst, &mi, NULL, &messenger);
    }
}

static bool has_ext(VkExtensionProperties *e, uint32_t n, const char *name) {
    for (uint32_t i = 0; i < n; i++) if (!strcmp(e[i].extensionName, name)) return true;
    return false;
}

// Discrete GPU first (LARECOMP_GPU=<index> picks one), needs Vulkan 1.3 and a queue that can present.
static void pick_device(void) {
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(g_vk.inst, &n, NULL);
    if (!n) FATAL("no Vulkan device\n");
    VkPhysicalDevice *pd = calloc(n, sizeof *pd);
    vkEnumeratePhysicalDevices(g_vk.inst, &n, pd);
    int want = getenv("LARECOMP_GPU") ? atoi(getenv("LARECOMP_GPU")) : -1, best = -1, best_score = -1;
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(pd[i], &p);
        LOG("[vk] gpu %u: %s (api %u.%u.%u)\n", i, p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion),
            VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion));
        if (p.apiVersion < VK_API_VERSION_1_3) continue;
        int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
        if ((int)i == want) score = 100;
        if (score > best_score) { best_score = score; best = (int)i; }
    }
    if (best < 0) FATAL("no Vulkan 1.3 device\n");
    g_vk.phys = pd[best];
    free(pd);
    vkGetPhysicalDeviceProperties(g_vk.phys, &g_vk.props);
    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.phys, &nq, NULL);
    VkQueueFamilyProperties *qp = calloc(nq, sizeof *qp);
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.phys, &nq, qp);
    g_vk.qfam = UINT32_MAX;
    for (uint32_t i = 0; i < nq; i++) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_vk.phys, i, g_vk.surface, &present);
        if ((qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && present) { g_vk.qfam = i; break; }
    }
    free(qp);
    if (g_vk.qfam == UINT32_MAX) FATAL("no graphics+present queue\n");
    LOG("[vk] using %s\n", g_vk.props.deviceName);
}

static bool g_fault_ext;
// After VK_ERROR_DEVICE_LOST: what the driver says faulted (VK_EXT_device_fault), into the log.
void vk_device_fault_report(void) {
    static int once;
    if (once++ || !g_fault_ext) return;
    PFN_vkGetDeviceFaultInfoEXT get = (PFN_vkGetDeviceFaultInfoEXT)vkGetDeviceProcAddr(g_vk.dev, "vkGetDeviceFaultInfoEXT");
    if (!get) return;
    VkDeviceFaultCountsEXT c = {.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
    if (get(g_vk.dev, &c, NULL) != VK_SUCCESS) { LOG("[vk] device fault: no info\n"); return; }
    VkDeviceFaultAddressInfoEXT addr[16];
    VkDeviceFaultVendorInfoEXT vend[16];
    if (c.addressInfoCount > 16) c.addressInfoCount = 16;
    if (c.vendorInfoCount > 16) c.vendorInfoCount = 16;
    c.vendorBinarySize = 0;
    VkDeviceFaultInfoEXT info = {.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT, .pAddressInfos = addr, .pVendorInfos = vend};
    if (get(g_vk.dev, &c, &info) != VK_SUCCESS && 0) return;
    LOG("[vk] device fault: %s\n", info.description);
    static const char *const kind[] = {"none", "read invalid", "write invalid", "execute invalid", "ip unknown", "ip invalid", "ip valid"};
    for (uint32_t i = 0; i < c.addressInfoCount; i++)
        LOG("[vk]   address %s %#llx (precision %#llx)\n", addr[i].addressType < 7 ? kind[addr[i].addressType] : "?",
            (unsigned long long)addr[i].reportedAddress, (unsigned long long)addr[i].addressPrecision);
    for (uint32_t i = 0; i < c.vendorInfoCount; i++)
        LOG("[vk]   vendor: %s (code %#llx data %#llx)\n", vend[i].description, (unsigned long long)vend[i].vendorFaultCode,
            (unsigned long long)vend[i].vendorFaultData);
}

static void create_device(void) {
    uint32_t ne = 0;
    vkEnumerateDeviceExtensionProperties(g_vk.phys, NULL, &ne, NULL);
    VkExtensionProperties *ep = calloc(ne, sizeof *ep);
    vkEnumerateDeviceExtensionProperties(g_vk.phys, NULL, &ne, ep);
    const char *ext[8];
    uint32_t n_ext = 0;
    ext[n_ext++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    g_vk.custom_border = has_ext(ep, ne, VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
    if (g_vk.custom_border) ext[n_ext++] = VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME;
    g_vk.divisor = has_ext(ep, ne, VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
    if (g_vk.divisor) ext[n_ext++] = VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME;
    g_vk.host_import = has_ext(ep, ne, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);  // guest heap as a GPU buffer (exec.c)
    if (g_vk.host_import) ext[n_ext++] = VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME;
    bool fault = has_ext(ep, ne, VK_EXT_DEVICE_FAULT_EXTENSION_NAME);  // device-lost diagnostics (vk_device_fault_report)
    if (fault) ext[n_ext++] = VK_EXT_DEVICE_FAULT_EXTENSION_NAME;
    g_fault_ext = fault;
#ifdef __ANDROID__
    // LARECOMP_SHADER_STATS=1: each graphics pipeline's compiled shaders described in the log (registers, waves,
    // spills, instructions: exec.c build_pipeline), for finding what makes a translated shader slow on this GPU
    g_vk.exec_stats = getenv("LARECOMP_SHADER_STATS") && has_ext(ep, ne, VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
    if (g_vk.exec_stats) ext[n_ext++] = VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME;
    // the guest heap as GPU memory mapped at its own address (exec.c vk_heap_place; Turnip has it)
    g_vk.map_placed = has_ext(ep, ne, VK_EXT_MAP_MEMORY_PLACED_EXTENSION_NAME) && has_ext(ep, ne, VK_KHR_MAP_MEMORY_2_EXTENSION_NAME);
    if (g_vk.map_placed) {
        ext[n_ext++] = VK_KHR_MAP_MEMORY_2_EXTENSION_NAME;
        ext[n_ext++] = VK_EXT_MAP_MEMORY_PLACED_EXTENSION_NAME;
    }
#endif
    free(ep);

    // features the backend relies on (all present on current desktop drivers)
    VkPhysicalDeviceFeatures2 have = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    vkGetPhysicalDeviceFeatures2(g_vk.phys, &have);
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT div = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT,
        .vertexAttributeInstanceRateDivisor = VK_TRUE, .vertexAttributeInstanceRateZeroDivisor = VK_FALSE};
    VkPhysicalDeviceCustomBorderColorFeaturesEXT cbc = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT, .customBorderColors = VK_TRUE,
        .customBorderColorWithoutFormat = VK_TRUE, .pNext = g_vk.divisor ? &div : NULL};
    VkPhysicalDeviceVulkan13Features f13 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                                            .dynamicRendering = VK_TRUE, .synchronization2 = VK_TRUE,
                                            .shaderDemoteToHelperInvocation = VK_TRUE, .shaderTerminateInvocation = VK_TRUE,
                                            .maintenance4 = VK_TRUE,
                                            .pNext = g_vk.custom_border ? (void *)&cbc : g_vk.divisor ? (void *)&div : NULL};
    VkPhysicalDeviceVulkan12Features f12 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                            .timelineSemaphore = VK_TRUE, .shaderInt8 = VK_TRUE, .samplerMirrorClampToEdge = VK_TRUE,
                                            .uniformAndStorageBuffer8BitAccess = VK_TRUE, .storageBuffer8BitAccess = VK_TRUE,
                                            .scalarBlockLayout = VK_TRUE, .pNext = &f13};
    VkPhysicalDeviceVulkan11Features f11 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                                            .storageBuffer16BitAccess = VK_TRUE, .pNext = &f12};
    VkPhysicalDeviceFaultFeaturesEXT ff = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT, .deviceFault = VK_TRUE,
                                           .pNext = &f11};
    VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = fault ? (void *)&ff : (void *)&f11};
    VkPhysicalDeviceFeatures *b = &f.features, *h = &have.features;
#ifdef __ANDROID__
    {   // Adreno has no uniformAndStorageBuffer8BitAccess, and no shader uses 8-bit uniform buffers
        // (docs/ANDROID-PORT.md, Fase 0): the 8-bit features only when the driver has them
        VkPhysicalDeviceVulkan12Features h12 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 h2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &h12};
        vkGetPhysicalDeviceFeatures2(g_vk.phys, &h2);
        f12.shaderInt8 = h12.shaderInt8;
        f12.uniformAndStorageBuffer8BitAccess = h12.uniformAndStorageBuffer8BitAccess;
        f12.storageBuffer8BitAccess = h12.storageBuffer8BitAccess;
    }
#endif
    b->independentBlend = h->independentBlend;
    b->dualSrcBlend = h->dualSrcBlend;
    b->fillModeNonSolid = h->fillModeNonSolid;
    b->samplerAnisotropy = h->samplerAnisotropy;
    b->textureCompressionBC = h->textureCompressionBC;
#ifdef __ANDROID__
    b->textureCompressionASTC_LDR = h->textureCompressionASTC_LDR;  // (Adreno: the game's ASTC textures as they are)
#endif
    b->shaderInt64 = h->shaderInt64;
    b->depthBiasClamp = h->depthBiasClamp;
    b->depthClamp = h->depthClamp;
    b->largePoints = h->largePoints;
    b->wideLines = h->wideLines;
    b->imageCubeArray = h->imageCubeArray;
    b->sampleRateShading = h->sampleRateShading;
    b->fragmentStoresAndAtomics = h->fragmentStoresAndAtomics;
    b->vertexPipelineStoresAndAtomics = h->vertexPipelineStoresAndAtomics;
    b->robustBufferAccess = h->robustBufferAccess;  // guest shaders index cbufs freely: keep OOB reads harmless
    b->shaderClipDistance = h->shaderClipDistance;
    if (!h->shaderInt64) LOG("[vk] warning: no shaderInt64 (64-bit address math in shaders)\n");
#ifdef __ANDROID__
    VkPhysicalDeviceMapMemoryPlacedFeaturesEXT placed = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT};
    if (g_vk.map_placed) {
        VkPhysicalDeviceMapMemoryPlacedFeaturesEXT pf = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT};
        VkPhysicalDeviceFeatures2 q = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &pf};
        vkGetPhysicalDeviceFeatures2(g_vk.phys, &q);
        VkPhysicalDeviceMapMemoryPlacedPropertiesEXT pp = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 qp = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &pp};
        vkGetPhysicalDeviceProperties2(g_vk.phys, &qp);
        g_vk.placed_align = pp.minPlacedMemoryMapAlignment;
        if (pf.memoryMapPlaced) {
            placed.memoryMapPlaced = VK_TRUE;
            placed.pNext = f.pNext;
            f.pNext = &placed;
        } else {
            g_vk.map_placed = false;
            n_ext -= 2;  // (the two map extensions were the last added)
        }
    }
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pex = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR, .pipelineExecutableInfo = VK_TRUE};
    if (g_vk.exec_stats) { pex.pNext = f.pNext; f.pNext = &pex; }
#endif
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = g_vk.qfam,
                                  .queueCount = 1, .pQueuePriorities = &prio};
    VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f, .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qi, .enabledExtensionCount = n_ext, .ppEnabledExtensionNames = ext};
    VK_CHECK(vkCreateDevice(g_vk.phys, &di, NULL, &g_vk.dev));
#ifdef __ANDROID__
    volkLoadDevice(g_vk.dev);
#endif
    vkGetDeviceQueue(g_vk.dev, g_vk.qfam, 0, &g_vk.queue);

    VmaAllocatorCreateInfo ai = {.physicalDevice = g_vk.phys, .device = g_vk.dev, .instance = g_vk.inst,
                                 .vulkanApiVersion = VK_API_VERSION_1_3};
#ifdef __ANDROID__
    VmaVulkanFunctions vf = {.vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr};
    ai.pVulkanFunctions = &vf;  // (VMA_DYNAMIC_VULKAN_FUNCTIONS: from the driver volk loaded)
#endif
    VK_CHECK(vmaCreateAllocator(&ai, &g_vk.vma));

    VkSemaphoreTypeCreateInfo st = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, .initialValue = 0};
    VkSemaphoreCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &st};
    VK_CHECK(vkCreateSemaphore(g_vk.dev, &si, NULL, &g_vk.timeline));
    VkCommandPoolCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                  .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = g_vk.qfam};
    VK_CHECK(vkCreateCommandPool(g_vk.dev, &pi, NULL, &g_vk.pool));

    // pipeline cache: cache/vk_pipelines.bin (written by vk_pipeline_cache_save)
    size_t sz = 0;
    void *data = NULL;
    FILE *fp = fopen("cache/vk_pipelines.bin", "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        sz = (size_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);
        data = malloc(sz);
        if (fread(data, 1, sz, fp) != sz) sz = 0;
        fclose(fp);
    }
    VkPipelineCacheCreateInfo pc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, .initialDataSize = sz,
                                    .pInitialData = data};
    if (vkCreatePipelineCache(g_vk.dev, &pc, NULL, &g_vk.pcache) != VK_SUCCESS) {
        pc.initialDataSize = 0;
        VK_CHECK(vkCreatePipelineCache(g_vk.dev, &pc, NULL, &g_vk.pcache));
    }
    free(data);
}

void vk_pipeline_cache_save(void) {
    size_t sz = 0;
    if (vkGetPipelineCacheData(g_vk.dev, g_vk.pcache, &sz, NULL) != VK_SUCCESS || !sz) return;
    void *data = malloc(sz);
    if (vkGetPipelineCacheData(g_vk.dev, g_vk.pcache, &sz, data) == VK_SUCCESS) {
        mkdir("cache", 0755);
        FILE *f = fopen("cache/vk_pipelines.bin.tmp", "wb");
        if (f) {
            bool ok = fwrite(data, 1, sz, f) == sz;
            fclose(f);
            if (ok) { remove("cache/vk_pipelines.bin"); rename("cache/vk_pipelines.bin.tmp", "cache/vk_pipelines.bin"); }
        }
    }
    free(data);
}

// ================================================================== swapchain
static VkPresentModeKHR present_mode(void) {
    // MAILBOX by default: the game paces itself (vi.c's emulated 60 Hz vsync), and with FIFO every submission
    // queued behind the one waiting for a window image, so the game's fence waits followed the presentation engine:
    // a throttled / captured / busy compositor slowed the whole game (4-21 fps with the GPU mostly idle; mailbox:
    // 60). No tearing. LARECOMP_VK_PRESENT=fifo|immediate|mailbox to choose.
    const char *e = getenv("LARECOMP_VK_PRESENT");
    VkPresentModeKHR want = !e || !strcmp(e, "mailbox") ? VK_PRESENT_MODE_MAILBOX_KHR
                          : !strcmp(e, "immediate") ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
    uint32_t n = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.phys, g_vk.surface, &n, NULL);
    VkPresentModeKHR m[16];
    if (n > 16) n = 16;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.phys, g_vk.surface, &n, m);
    for (uint32_t i = 0; i < n; i++) if (m[i] == want) return want;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static void create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.phys, g_vk.surface, &caps));
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
#ifdef __ANDROID__
    // The surface reports the panel's own (portrait) orientation; the window is landscape: images of the window's
    // size, turned by the compositor (pre-rotation in the app would spare it that: docs/ANDROID-PORT.md, Fase 3)
    VkExtent2D ext = {(uint32_t)w, (uint32_t)h};
    VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    LOG("[vk] swapchain: surface %ux%u transform %#x, window %dx%d, present mode %d, images %u-%u\n", caps.currentExtent.width,
        caps.currentExtent.height, caps.currentTransform, w, h, present_mode(), caps.minImageCount, caps.maxImageCount);
#else
    VkExtent2D ext = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : (VkExtent2D){(uint32_t)w, (uint32_t)h};
    VkSurfaceTransformFlagBitsKHR transform = caps.currentTransform;
#endif
    if (!ext.width || !ext.height) return;  // minimised
    uint32_t nf = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.phys, g_vk.surface, &nf, NULL);
    VkSurfaceFormatKHR fmts[64];
    if (nf > 64) nf = 64;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.phys, g_vk.surface, &nf, fmts);
    // scanout shows the bytes the game wrote (see the raw views in the present pass): a UNORM swapchain like Metal's
    VkSurfaceFormatKHR sf = fmts[0];
    for (uint32_t i = 0; i < nf; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { sf = fmts[i]; break; }
    uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount && count > caps.maxImageCount) count = caps.maxImageCount;
    if (count > 8) count = 8;
    VkSwapchainKHR old = g_vk.swapchain;
    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = g_vk.surface, .minImageCount = count,
        .imageFormat = sf.format, .imageColorSpace = sf.colorSpace, .imageExtent = ext, .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = present_mode(), .clipped = VK_TRUE,
        .oldSwapchain = old};
    VK_CHECK(vkCreateSwapchainKHR(g_vk.dev, &ci, NULL, &g_vk.swapchain));
    for (uint32_t i = 0; i < g_vk.sc_count; i++) vkDestroyImageView(g_vk.dev, g_vk.sc_views[i], NULL);
    if (old) vkDestroySwapchainKHR(g_vk.dev, old, NULL);
    g_vk.sc_format = sf.format;
    g_vk.sc_extent = ext;
    g_vk.sc_count = 8;
    vkGetSwapchainImagesKHR(g_vk.dev, g_vk.swapchain, &g_vk.sc_count, g_vk.sc_images);
    for (uint32_t i = 0; i < g_vk.sc_count; i++) {
        VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = g_vk.sc_images[i],
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = sf.format,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        VK_CHECK(vkCreateImageView(g_vk.dev, &vi, NULL, &g_vk.sc_views[i]));
    }
}

static void recreate_swapchain(void) {
    vkDeviceWaitIdle(g_vk.dev);
    create_swapchain();
}

#ifdef __ANDROID__
// The app in the background loses its window surface (SDL clears the window's ANativeWindow, and gives the new one
// when it comes back): the Vulkan surface and swapchain are made again for the new window. Frames in between are
// skipped. Before this, everything after a trip to the background stayed black (every acquire: SURFACE_LOST).
static struct ANativeWindow *surface_aw;  // the window g_vk.surface was made for
static bool surface_lost;

static struct ANativeWindow *android_window(void) {
    return SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
}

static void android_create_surface(struct ANativeWindow *aw) {
    VkAndroidSurfaceCreateInfoKHR si = {.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window = aw};
    VK_CHECK(vkCreateAndroidSurfaceKHR(g_vk.inst, &si, NULL, &g_vk.surface));
    surface_aw = aw;
}

static bool android_surface_ready(void) {  // false: no window to draw into now
    struct ANativeWindow *aw = android_window();
    if (!aw) { surface_lost = true; return false; }
    if (aw == surface_aw && !surface_lost) return true;
    vkDeviceWaitIdle(g_vk.dev);
    for (uint32_t i = 0; i < g_vk.sc_count; i++) vkDestroyImageView(g_vk.dev, g_vk.sc_views[i], NULL);
    g_vk.sc_count = 0;
    if (g_vk.swapchain) vkDestroySwapchainKHR(g_vk.dev, g_vk.swapchain, NULL);
    g_vk.swapchain = VK_NULL_HANDLE;
    vkDestroySurfaceKHR(g_vk.inst, g_vk.surface, NULL);
    android_create_surface(aw);
    surface_lost = false;
    LOG("[vk] window surface made again (the app came back from the background)\n");
    create_swapchain();
    return g_vk.swapchain != VK_NULL_HANDLE;
}
#endif

// ================================================================== frames (start screen and game)
// Acquire semaphores rotate per frame in flight; a slot is reused once the submission that waited on it is done.
#define FRAMES 3
static VkSemaphore acquire_sem[FRAMES], done_sem[8];
static uint64_t slot_seq[FRAMES];
static int slot;

static VkSemaphore binary_semaphore(void) {
    VkSemaphoreCreateInfo si = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore s;
    VK_CHECK(vkCreateSemaphore(g_vk.dev, &si, NULL, &s));
    return s;
}

static void image_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier2 b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                               .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .oldLayout = from, .newLayout = to, .image = img,
                               .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED};
    VkDependencyInfo di = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b};
    vkCmdPipelineBarrier2(cb, &di);
}

typedef struct { uint32_t image; VkSemaphore acquired; bool ok; } Frame;
static Frame cur_frame;

// Acquire the next window image and begin rendering into it (cleared to `clear`). Returns false when there is
// nothing to draw into (minimised window).
bool vk_frame_begin(const float clear[4]) {
    cur_frame.ok = false;
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (!w || !h) return false;  // minimised
#ifdef __ANDROID__
    if (!android_surface_ready()) return false;
#endif
    if (!g_vk.swapchain || (uint32_t)w != g_vk.sc_extent.width || (uint32_t)h != g_vk.sc_extent.height) recreate_swapchain();
    if (!g_vk.swapchain) return false;
    static int trace = -1;
    if (trace < 0) trace = getenv("LARECOMP_VK_TRACE") != NULL;
    if (trace) LOG("[vk] frame begin %dx%d (swapchain %ux%u)\n", w, h, g_vk.sc_extent.width, g_vk.sc_extent.height);
    slot = (slot + 1) % FRAMES;
    if (!acquire_sem[slot]) acquire_sem[slot] = binary_semaphore();
    vk_wait_seq(slot_seq[slot]);
    VkResult r = vkAcquireNextImageKHR(g_vk.dev, g_vk.swapchain, UINT64_MAX, acquire_sem[slot], VK_NULL_HANDLE, &cur_frame.image);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        r = vkAcquireNextImageKHR(g_vk.dev, g_vk.swapchain, UINT64_MAX, acquire_sem[slot], VK_NULL_HANDLE, &cur_frame.image);
    }
#ifdef __ANDROID__
    if (r == VK_ERROR_SURFACE_LOST_KHR) { surface_lost = true; return false; }  // (made again at the next frame)
#endif
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) { LOG("[vk] acquire failed: %d\n", r); return false; }
    cur_frame.acquired = acquire_sem[slot];
    slot_seq[slot] = vk_pending_seq();
    VkCommandBuffer cb = vk_cmd();
    image_barrier(cb, g_vk.sc_images[cur_frame.image], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo ca = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = g_vk.sc_views[cur_frame.image],
                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                    .clearValue.color.float32 = {clear[0], clear[1], clear[2], clear[3]}};
    VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, g_vk.sc_extent}, .layerCount = 1,
                          .colorAttachmentCount = 1, .pColorAttachments = &ca};
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp = {0, 0, (float)g_vk.sc_extent.width, (float)g_vk.sc_extent.height, 0, 1};
    VkRect2D sc = {{0, 0}, g_vk.sc_extent};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
    cur_frame.ok = true;
    return true;
}

// Passes of their own between vk_frame_begin and vk_frame_end (exec.c post passes): end the window image's
// rendering, and begin it again keeping what was drawn.
void vk_frame_suspend(void) {
    if (cur_frame.ok) vkCmdEndRendering(vk_cmd());
}
void vk_frame_resume(void) {
    if (!cur_frame.ok) return;
    VkCommandBuffer cb = vk_cmd();
    VkRenderingAttachmentInfo ca = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = g_vk.sc_views[cur_frame.image],
                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, g_vk.sc_extent}, .layerCount = 1,
                          .colorAttachmentCount = 1, .pColorAttachments = &ca};
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp = {0, 0, (float)g_vk.sc_extent.width, (float)g_vk.sc_extent.height, 0, 1};
    VkRect2D sc = {{0, 0}, g_vk.sc_extent};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
}

VkExtent2D vk_frame_extent(void) { return g_vk.sc_extent; }
void *vk_frame_cmd(void) { return (void *)vk_cmd(); }

static void save_window_image(VkCommandBuffer cb, VkSpan *out) {
    uint32_t w = g_vk.sc_extent.width, h = g_vk.sc_extent.height;
    *out = vk_readback((uint64_t)w * h * 4);
    image_barrier(cb, g_vk.sc_images[cur_frame.image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy rg = {.bufferOffset = out->off, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                            .imageExtent = {w, h, 1}};
    vkCmdCopyImageToBuffer(cb, g_vk.sc_images[cur_frame.image], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out->buf, 1, &rg);
    image_barrier(cb, g_vk.sc_images[cur_frame.image], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

// End rendering into the window image and present it. shot_path: also save the window image as PNG.
void vk_frame_end(const char *shot_path) {
    if (!cur_frame.ok) return;
    VkCommandBuffer cb = vk_cmd();
    vkCmdEndRendering(cb);
    VkSpan shot = {0};
    if (shot_path) save_window_image(cb, &shot);
    image_barrier(cb, g_vk.sc_images[cur_frame.image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    uint32_t img = cur_frame.image;
    if (!done_sem[img]) done_sem[img] = binary_semaphore();
    uint64_t seq = vk_pending_seq();
    vk_submit_extra(cur_frame.acquired, done_sem[img], VK_NULL_HANDLE);
    VkPresentInfoKHR pi = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &done_sem[img],
                           .swapchainCount = 1, .pSwapchains = &g_vk.swapchain, .pImageIndices = &img};
    VkResult r = vkQueuePresentKHR(g_vk.queue, &pi);
#ifdef __ANDROID__
    if (r == VK_ERROR_OUT_OF_DATE_KHR) recreate_swapchain();  // (SUBOPTIMAL: every frame of an unrotated swapchain)
    else if (r == VK_ERROR_SURFACE_LOST_KHR) surface_lost = true;
#else
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) recreate_swapchain();
#endif
    cur_frame.ok = false;
    if (shot_path) {
        vk_wait_seq(seq);
        uint32_t w = g_vk.sc_extent.width, h = g_vk.sc_extent.height;
        uint8_t *px = shot.ptr;
        bool bgr = g_vk.sc_format == VK_FORMAT_B8G8R8A8_UNORM || g_vk.sc_format == VK_FORMAT_B8G8R8A8_SRGB;
        for (uint64_t i = 0; bgr && i < (uint64_t)w * h; i++) { uint8_t t = px[i * 4]; px[i * 4] = px[i * 4 + 2]; px[i * 4 + 2] = t; }
        for (uint64_t i = 0; i < (uint64_t)w * h; i++) px[i * 4 + 3] = 255;
        vk_save_png(shot_path, px, w, h);
    }
}

void vk_save_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h) {
    SDL_Surface *s = SDL_CreateSurfaceFrom((int)w, (int)h, SDL_PIXELFORMAT_ABGR8888, (void *)rgba, (int)w * 4);
    if (!s || !SDL_SavePNG(s, path)) LOG("[vk] saving %s failed: %s\n", path, SDL_GetError());
    else LOG("[vk] saved %s (%ux%u)\n", path, w, h);
    SDL_DestroySurface(s);
}

// ================================================================== game presentation (metal.m backend_present)
// Draws the game's image into the window with the widescreen crop, then the overlay menu.
extern void vk_present_draw(VkCommandBuffer cb, TexBackend *b, float vx, float vy, float vw, float vh, const float pv[8]);
void renderer_submit(void);
void renderer_set_scale(float sx, float sy);
uint64_t renderer_present_count(void);
void renderer_save_texture_tex(TexBackend *b, const char *path);

static uint64_t frames, total_frames, g_frame_counter;
#ifdef __ANDROID__
// LARECOMP_VK_ASYNC (vulkan/async.c): the game's threads count the presents they queued, the render thread the ones
// it ran - each side's "this frame" stays its own
static uint64_t g_game_frames;
bool vk_async_on(void);
bool vk_async_is_render_thread(void);
void vk_async_present(NvnTexture *t);
void vk_async_drain(void);
uint64_t renderer_present_count(void) { return vk_async_on() && !vk_async_is_render_thread() ? g_game_frames : total_frames; }
#else
uint64_t renderer_present_count(void) { return total_frames; }
#endif
uint64_t vk_frame_counter(void) { return g_frame_counter; }
static float g_shot_crop[4] = {0, 0, 1, 1};
static double last_fps_time;
static NvnTexture *g_last_presented;

const float *vk_shot_crop(void) { return g_shot_crop; }

void backend_present(NvnTexture *t);
#ifdef __ANDROID__
// The game's half of a present: pacing, input, tests - on the game's thread (its frame: renderer_present_count)
static void present_game_half(void) {
    extern void vk_pace_frame(void);
    vk_pace_frame();  // (exec.c: the game's 30 fps mode when frames don't fit 60)
    extern void input_frame(uint64_t frame);
    input_frame(renderer_present_count());
    static long slow_ms = -1;
    if (slow_ms < 0) slow_ms = getenv("LARECOMP_SLOW_FRAME_MS") ? atol(getenv("LARECOMP_SLOW_FRAME_MS")) : 0;
    static long slow_until = -1;  // LARECOMP_SLOW_FRAME_UNTIL=<frame>: the stall ends there (frame pacing recovery tests)
    if (slow_until < 0) slow_until = getenv("LARECOMP_SLOW_FRAME_UNTIL") ? atol(getenv("LARECOMP_SLOW_FRAME_UNTIL")) : 0;
    if (slow_ms > 0 && renderer_present_count() > 2600 && (!slow_until || (long)renderer_present_count() < slow_until)) SDL_Delay((Uint32)slow_ms);
    extern void fnprof_frame(uint64_t frame);
    fnprof_frame(renderer_present_count());
    extern void warp_frame(uint64_t frame);
    warp_frame(renderer_present_count());
    extern void reboot_test_frame(uint64_t frame);  // runtime/main.c (LARECOMP_REBOOT_AT)
    reboot_test_frame(renderer_present_count());
    extern void save_editor_frame(uint64_t frame);
    save_editor_frame(renderer_present_count());
}

void vk_present_now(NvnTexture *t);
void backend_present(NvnTexture *t) {
        if (nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL)
        FATAL("[gles-backend] backend_present requires GLES presentation\n");
    if (!vk_async_on()) {
        vk_present_now(t);
        return;
    }
    g_game_frames++;
    vk_async_present(t);  // (the window's half, on the render thread: vk_present_now)
    present_game_half();
}
#endif
void backend_represent(void) {
    if (g_last_presented) backend_present(g_last_presented);
    else SDL_Delay(16);
}

#ifdef __ANDROID__
void vk_present_now(NvnTexture *t) {  // (the window's half of backend_present)
#else
void backend_present(NvnTexture *t) {
#endif
    g_last_presented = t;
    TexBackend *b = vk_tex(t, true);
    b->presented = true;
    vk_end_pass();
    static const float black[4] = {0, 0, 0, 1};
    uint64_t t0 = SDL_GetTicksNS();
    bool have = vk_frame_begin(black);
    uint64_t nd = SDL_GetTicksNS() - t0;
    if (nd > 30000000) {
        static int warned;
        if (warned++ < 20) LOG("[vk] acquire blocked %llums\n", (unsigned long long)(nd / 1000000));
    }
    if (have) {
        double dw = g_vk.sc_extent.width, dh = g_vk.sc_extent.height;
        extern void widescreen_frame_end(uint64_t frame);
        widescreen_frame_end(total_frames);
        extern void widescreen_set_factor(float f);
        extern bool widescreen_allowed(void);
        // render scale + placement in the window: runtime/nvn/output_scale.c (Video tab "Internal resolution");
        // no GPU timer here (-1): dynamic resolution goes by the frame time
        OutScale os;
        output_scale(dw, dh, widescreen_allowed(), SDL_GetTicksNS() / 1e9, -1, &os);
        double sx = os.sx, sy = os.sy, vx = os.vx, vy = os.vy, vw = os.vw, vh = os.vh, shape = os.shape;
        // interiors / dungeons: their 16:9 centre, or the part inside a room wider than one screen
        extern void widescreen_present_crop(uint64_t, double, float *, double *, double *, double *, double *);
        float crop[4];  // uv origin and size
        widescreen_present_crop(total_frames, shape, crop, &vx, &vy, &vw, &vh);
        widescreen_set_factor((float)shape);
        renderer_set_scale((float)sx, (float)sy);
        VkCommandBuffer cb = vk_cmd();
        float pv[8] = {renderer_origin_lower_left() ? 1.0f : 0.0f, 0, 0, 0, crop[0], crop[1], crop[2], crop[3]};
        memcpy(g_shot_crop, crop, sizeof crop);
        vk_present_draw(cb, b, (float)vx, (float)vy, (float)vw, (float)vh, pv);
        static long menu_at = -1;
        if (menu_at < 0) menu_at = getenv("LARECOMP_MENU_AT") ? atol(getenv("LARECOMP_MENU_AT")) : 0;
        if (menu_at && (long)total_frames + 1 == menu_at) { extern void menu_test_open(void); menu_test_open(); }
        menu_render((void *)cb, g_vk.sc_extent.width, g_vk.sc_extent.height);
        static long win_shot = -1;
        if (win_shot < 0) win_shot = getenv("LARECOMP_WINDOW_SHOT") ? atol(getenv("LARECOMP_WINDOW_SHOT")) : 0;
        bool shot = win_shot && (long)total_frames + 1 == win_shot;
        vk_frame_end(shot ? "build/window.png" : NULL);
        if (shot && getenv("LARECOMP_EXIT_AFTER_SHOT")) { fflush(stderr); _exit(0); }
    } else {
        renderer_submit();
    }
    extern void vk_prof_frame(uint64_t frame, uint64_t present_ns);
    vk_prof_frame(total_frames, SDL_GetTicksNS() - t0);
    frames++;
    total_frames++;
    g_frame_counter++;
#ifdef __ANDROID__
    if (!vk_async_on()) present_game_half();  // (same place as before; LARECOMP_VK_ASYNC: on the game's thread)
#else
    extern void vk_pace_frame(void);
    vk_pace_frame();  // (exec.c: the game's 30 fps mode when frames don't fit 60)
    extern void input_frame(uint64_t frame);
    input_frame(total_frames);
    static long slow_ms = -1;
    if (slow_ms < 0) slow_ms = getenv("LARECOMP_SLOW_FRAME_MS") ? atol(getenv("LARECOMP_SLOW_FRAME_MS")) : 0;
    static long slow_until = -1;  // LARECOMP_SLOW_FRAME_UNTIL=<frame>: the stall ends there (frame pacing recovery tests)
    if (slow_until < 0) slow_until = getenv("LARECOMP_SLOW_FRAME_UNTIL") ? atol(getenv("LARECOMP_SLOW_FRAME_UNTIL")) : 0;
    if (slow_ms > 0 && total_frames > 2600 && (!slow_until || (long)total_frames < slow_until)) SDL_Delay((Uint32)slow_ms);
    extern void fnprof_frame(uint64_t frame);
    fnprof_frame(total_frames);
    extern void warp_frame(uint64_t frame);
    warp_frame(total_frames);
    extern void reboot_test_frame(uint64_t frame);  // runtime/main.c (LARECOMP_REBOOT_AT)
    reboot_test_frame(total_frames);
    extern void save_editor_frame(uint64_t frame);
    save_editor_frame(total_frames);
#endif
    extern void vk_level_change_check(void);
    vk_level_change_check();
    static long shot_every = -1;
    if (shot_every < 0) { const char *e = getenv("LARECOMP_SCREENSHOT_EVERY"); shot_every = e ? atol(e) : 0; }
    extern bool g_debug_frame;
    static long debug_at = -1;
    if (debug_at < 0) { const char *e = getenv("LARECOMP_DEBUG_FRAME"); debug_at = e ? atol(e) : 0; }
    extern volatile int g_debug_frame_request;
    if (g_debug_frame_request) { g_debug_frame_request = 0; debug_at = (long)total_frames + 1; }
    // LARECOMP_DEBUG_FRAMES=<n> (with LARECOMP_DEBUG_FRAME / F9): dump n consecutive frames (build/pass_<frame>_NNN.png)
    static long debug_n = -1;
    if (debug_n < 0) { const char *e = getenv("LARECOMP_DEBUG_FRAMES"); debug_n = e && atol(e) > 1 ? atol(e) : 1; }
    g_debug_frame = debug_at && total_frames >= (uint64_t)debug_at && total_frames < (uint64_t)debug_at + (uint64_t)debug_n;
    {   // LARECOMP_GPUPROF_AT=<frame>[,<frame>...]: GPU time per pass of those frames ("[gpuprof]" in the log)
        extern bool g_gpuprof_frame;
        extern void vk_gpuprof_report(void);
        if (g_gpuprof_frame) { g_gpuprof_frame = false; vk_gpuprof_report(); }
        const char *e = getenv("LARECOMP_GPUPROF_AT");
        for (const char *p = e; p && *p;) {
            long at = strtol(p, (char **)&p, 10);
            if (at == (long)total_frames) { g_gpuprof_frame = true; break; }
            if (*p == ',') p++; else break;
        }
    }
    if (g_debug_frame) LOG("===== debug frame %llu =====\n", (unsigned long long)total_frames);
    {   // LARECOMP_SHOT_AT=<frame>,<frame>,... (ascending): the game image at those frames -> build/frames/frame_N.png
        static const char *next;
        static int init;
        if (!init) { init = 1; next = getenv("LARECOMP_SHOT_AT"); if (next) mkdir("build/frames", 0755); }
        while (next && *next) {
            long at = strtol(next, (char **)&next, 10);
            if ((long)total_frames < at) { while (next > getenv("LARECOMP_SHOT_AT") && next[-1] != ',') next--; break; }
            if ((long)total_frames == at) {
                char path[256];
                snprintf(path, sizeof path, "build/frames/frame_%05llu.png", (unsigned long long)total_frames);
                renderer_save_texture_tex(b, path);
            }
            if (*next == ',') next++;
        }
    }
    static long shot_from = -1;
    if (shot_from < 0) { const char *e = getenv("LARECOMP_SCREENSHOT_FROM"); shot_from = e ? atol(e) : 0; }
    if (shot_every && total_frames % shot_every == 0 && (long)total_frames >= shot_from) {
        char path[256];
        if (shot_from) mkdir("build/frames", 0755);
        snprintf(path, sizeof path, shot_from ? "build/frames/frame_%05llu.png" : "build/frame_%05llu.png",
                 (unsigned long long)total_frames);
        renderer_save_texture_tex(b, path);
        // a series (SCREENSHOT_FROM) runs on until LARECOMP_WINDOW_SHOT exits
        if (getenv("LARECOMP_EXIT_AFTER_SHOT") && !shot_from) { fflush(stderr); _exit(0); }
    }
    double now = SDL_GetTicks() / 1000.0;
    static double fps_iv = -1, prev_present, max_gap;
    if (fps_iv < 0) fps_iv = getenv("LARECOMP_FPS_INTERVAL") ? atof(getenv("LARECOMP_FPS_INTERVAL")) : 2.0;
    if (prev_present > 0 && now - prev_present > max_gap) max_gap = now - prev_present;
    prev_present = now;
    if (now - last_fps_time >= fps_iv) {
        extern const char *game_level_name(void);
        if (last_fps_time > 0)
            LOG("[vk] t=%.1f frame %llu %.1f fps max %.0fms level %s (%s)\n", now, (unsigned long long)total_frames,
                frames / (now - last_fps_time), max_gap * 1000.0, game_level_name(), renderer_stats());
        max_gap = 0;
        frames = 0;
        last_fps_time = now;
    }
}

// ================================================================== UI thread
void vk_exec_init(void);
void vk_shaders_init(void);

// exit() from any thread: let the GPU finish, keep the pipeline cache, then end the process without the DLL
// teardown (driver / validation-layer threads can stall it for minutes while the game's threads still run).
static void quick_exit_handler(void) {
    static int trace = -1;
    if (trace < 0) trace = getenv("LARECOMP_VK_TRACE") != NULL;
    if (trace) LOG("[vk] exit: waiting for the GPU\n");
    if (g_vk.dev) {
        vkDeviceWaitIdle(g_vk.dev);
        if (trace) LOG("[vk] exit: saving the pipeline cache\n");
        vk_pipeline_cache_save();
    }
    if (trace) LOG("[vk] exit: terminating\n");
    fflush(stdout);
    fflush(stderr);
#if defined(_WIN32) || defined(__linux__)
    plat_terminate();
#endif
}

#ifdef __ANDROID__
static void las_gles_window_probe(void) {
    LOG("[gles-probe] starting dedicated game-window probe\n");

    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        FATAL("[gles-probe] SDL_Init: %s\n", SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Window *probe_window = SDL_CreateWindow(
        "LAS Recomp GLES Probe",
        1280,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
    );

    if (!probe_window) {
        FATAL("[gles-probe] SDL_CreateWindow: %s\n", SDL_GetError());
    }

    SDL_GLContext probe_context = SDL_GL_CreateContext(probe_window);

    if (!probe_context) {
        FATAL("[gles-probe] SDL_GL_CreateContext: %s\n", SDL_GetError());
    }

    if (!SDL_GL_MakeCurrent(probe_window, probe_context)) {
        FATAL("[gles-probe] SDL_GL_MakeCurrent: %s\n", SDL_GetError());
    }

    const char *version = (const char *)glGetString(GL_VERSION);
    const char *renderer = (const char *)glGetString(GL_RENDERER);

    LOG("[gles-probe] GL_VERSION=%s\n", version ? version : "unknown");
    LOG("[gles-probe] GL_RENDERER=%s\n", renderer ? renderer : "unknown");

    LOG("[gles-probe] entering persistent frame loop\n");

    SDL_GL_SetSwapInterval(1);

    for (int frame = 0; frame < 180; frame++) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                LOG("[gles-probe] quit requested\n");
                break;
            }
        }

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(probe_window, &width, &height);

        glViewport(0, 0, width, height);

        float pulse = (float)(frame % 60) / 60.0f;

        glClearColor(
            0.08f,
            0.12f + pulse * 0.15f,
            0.18f,
            1.0f
        );

        glClear(GL_COLOR_BUFFER_BIT);

        SDL_GL_SwapWindow(probe_window);

        GLenum error = glGetError();

        if (error != GL_NO_ERROR) {
            LOG("[gles-probe] GL error frame=%d code=0x%x\n", frame, error);
        }

        if (frame % 30 == 0) {
            LOG("[gles-probe] frame=%d size=%dx%d\n", frame, width, height);
        }

        SDL_Delay(16);
    }

    LOG("[gles-probe] persistent frame loop completed\n");

    SDL_GL_DestroyContext(probe_context);
    SDL_DestroyWindow(probe_window);

    LOG("[gles-probe] completed; Vulkan backend unchanged\n");

    fflush(stdout);
    fflush(stderr);
    exit(0);
}
#endif


void ui_window_init(void) {
#ifdef __ANDROID__
    if (nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL) {
        LOG("[gles-backend] persistent GLES startup selected\n");
        LOG("[gles-backend] Vulkan initialization disabled\n");

        SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
            FATAL("[gles-backend] SDL_Init: %s\n", SDL_GetError());

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

        window = SDL_CreateWindow(
            "LAS Recomp GLES Experimental",
            1280, 720,
            SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
        );

        if (!window)
            FATAL("[gles-backend] SDL_CreateWindow: %s\n", SDL_GetError());

        g_gles_context = SDL_GL_CreateContext(window);

        if (!g_gles_context)
            FATAL("[gles-backend] SDL_GL_CreateContext: %s\n", SDL_GetError());

        if (!SDL_GL_MakeCurrent(window, g_gles_context))
            FATAL("[gles-backend] SDL_GL_MakeCurrent: %s\n", SDL_GetError());

        LOG("[gles-backend] GL_VERSION=%s\n", glGetString(GL_VERSION));
        LOG("[gles-backend] GL_RENDERER=%s\n", glGetString(GL_RENDERER));
        extern void gles_e140_framebuffer_probe(void);
        gles_e140_framebuffer_probe();
        extern void gles_e153_framebuffer_probe(void);
        gles_e153_framebuffer_probe();
        extern int gles_e157_snapshot_registry_selftest(void);
        int e157_ok = gles_e157_snapshot_registry_selftest();
        LOG("[E157] startup registry selftest=%s\n",
            e157_ok ? "PASS" : "FAIL");
        extern int gles_e156_texture_snapshot_selftest(void);
        int e156_ok = gles_e156_texture_snapshot_selftest();
        LOG("[E156] startup snapshot selftest=%s\n",
            e156_ok ? "PASS" : "FAIL");

        SDL_GL_SetSwapInterval(1);

        LOG("[gles-backend] persistent context ready\n");

        /*
         * E118 synthetic GLES handshake.
         * The handle is a sentinel, never a guest command pointer.
         * E117 will reject it without dereferencing it.
         */
        {
            uint64_t e118_sequence = 0;
            GlesSubmissionState e118_state = GLES_SUBMISSION_PENDING;

            if (!gles_submission_enqueue(0xE118ULL, &e118_sequence))
                FATAL("[gles-backend] E118 synthetic enqueue failed\n");

            LOG("[gles-backend] E118 synthetic queued sequence=%llu\n",
                (unsigned long long)e118_sequence);
        }
        return;
    }
    if (getenv("LARECOMP_GLES_WINDOW_PROBE") ||
        access("/sdcard/Android/data/com.lasrecomp.android/files/gles-window-probe", F_OK) == 0) {
        las_gles_window_probe();
    }
#endif
    if (window) return;
    atexit(quick_exit_handler);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");  // (the back button opens the menu: ui/touch_controls.mm)
#endif
    settings_load();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) FATAL("SDL_Init: %s\n", SDL_GetError());
    int win_w = 1280, win_h = 720;
    if (getenv("LARECOMP_WINDOW")) sscanf(getenv("LARECOMP_WINDOW"), "%dx%d", &win_w, &win_h);
    SDL_WindowFlags wflags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __ANDROID__
    wflags |= SDL_WINDOW_FULLSCREEN;  // (immersive: no system bars)
#endif
    if (getenv("LARECOMP_HIDDEN")) wflags |= SDL_WINDOW_HIDDEN;
    {   // scripted test runs (user input ignored: runtime/hle/input.c input_user_blocked) open without taking the
        // keyboard focus from whatever the player is doing
        const char *v = getenv("LARECOMP_NO_USER_INPUT");
        bool scripted = v ? atoi(v) != 0 : (getenv("LARECOMP_INPUT") || getenv("LARECOMP_DOORJUMP_TEST") || getenv("LARECOMP_PLACE") ||
                                            getenv("LARECOMP_HOST_INPUT") || getenv("LARECOMP_BIND_TEST"));
        if (scripted) SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
    }
    window = SDL_CreateWindow("LAS Recomp", win_w, win_h, wflags);
    if (!window) FATAL("SDL_CreateWindow: %s\n", SDL_GetError());
    rt_window_icon(window);
    // the Fullscreen setting applies from the start screen on (not only once the game runs); test windows stay windows
#ifdef __ANDROID__
    SDL_SetWindowFullscreen(window, true);  // (always: the whole screen, system bars hidden)
#else
    if (g_settings.fullscreen && !getenv("LARECOMP_WINDOW") && !getenv("LARECOMP_HIDDEN")) SDL_SetWindowFullscreen(window, true);
#endif
    create_instance();
#ifdef __ANDROID__
    {   // (through the driver volk loaded: SDL would use the system loader's)
        struct ANativeWindow *aw = android_window();
        if (!aw) FATAL("no Android window\n");
        android_create_surface(aw);
    }
#else
    if (!SDL_Vulkan_CreateSurface(window, g_vk.inst, NULL, &g_vk.surface)) FATAL("SDL_Vulkan_CreateSurface: %s\n", SDL_GetError());
#endif
    pick_device();
    create_device();
    create_swapchain();
    vk_exec_init();
    vk_shaders_init();
}

SDL_Window *ui_window(void) { return window; }

#ifdef __ANDROID__
// For the performance overlay (ui/perf_overlay.mm): the GPU and its driver, short ("Adreno (TM) 830 - Turnip").
const char *vk_gpu_desc(void) {
    static char s[160];
    if (s[0] || !g_vk.phys) return s;
    VkPhysicalDeviceDriverProperties d = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 p = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &d};
    vkGetPhysicalDeviceProperties2(g_vk.phys, &p);
    const char *drv = strstr(d.driverName, "turnip") ? "Turnip" : strstr(d.driverName, "Qualcomm") ? "Qualcomm" : d.driverName;
    snprintf(s, sizeof s, "%s - %s", p.properties.deviceName, drv);
    return s;
}

// Qualcomm's own driver (not Turnip): its shader compiler crashed on the structured control flow (vulkan/exec.c)
bool vk_driver_is_qualcomm(void) {
    static int q = -1;
    if (q < 0 && g_vk.phys) {
        VkPhysicalDeviceDriverProperties d = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 p = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &d};
        vkGetPhysicalDeviceProperties2(g_vk.phys, &p);
        q = d.driverID == VK_DRIVER_ID_QUALCOMM_PROPRIETARY;
    }
    return q > 0;
}
#endif

void backend_init(void) {
    while (!atomic_load(&ui_ready)) SDL_Delay(1);
#ifdef __ANDROID__
    if (nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL) {
        LOG("[gles-backend] guest NVN initialization released\n");
        return;
    }
#endif
    LOG("[vk] %s, swapchain %ux%u\n", g_vk.props.deviceName, g_vk.sc_extent.width, g_vk.sc_extent.height);
}

// ---- in-place reboot (runtime/main.c teardown; main thread, guest threads stopped)
void backend_reset(void) {
#ifdef __ANDROID__
    if (nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL) {
        // Called on the UI thread after kernel_stop_threads().
        // Preserve the GLES context and window across in-place reboot.
        // GLES resource reset will be integrated here later.
        LOG("[gles-backend] reboot boundary reached; context preserved\n");
        return;
    }
#endif
    if (!g_vk.dev) return;
#ifdef __ANDROID__
    vk_async_drain();
#endif
    vkDeviceWaitIdle(g_vk.dev);
    extern void vk_textures_reset(void);
    extern void vk_exec_reset(void);
    vk_textures_reset();
    vk_exec_reset();
    g_last_presented = NULL;
}
void backend_guest_memory_reset(void) {}  // (exec.c re-imports the heap on first use)
void backend_clear_window(void) {
#ifdef __ANDROID__
    if (nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL) {
        LOG("[gles-backend] skipping Vulkan window clear\n");
        return;
    }
#endif
    static const float black[4] = {0, 0, 0, 1};
#ifdef __ANDROID__
    vk_async_drain();
#endif
    if (g_vk.dev && vk_frame_begin(black)) vk_frame_end(NULL);
}

#ifdef __ANDROID__
void vk_wait_ready(void) {
    while (!atomic_load(&ui_ready)) SDL_Delay(1);
}
#endif

int ui_run(void) {
    ui_window_init();
    atomic_store(&ui_ready, true);
    for (;;) {
        if (rt_reboot_requested()) return rt_reboot_requested();  // (runtime/main.c tears the game down)
        SDL_Event e;
#ifdef __ANDROID__
        // (SDL's wait doesn't sleep on Android: it kept a core busy updating joysticks and reading the clock - 40% of
        // the app's CPU in a profile. Poll what's there, then sleep: input at most 4 ms later.)
        static bool polled;
        if (polled) SDL_Delay(4);
        polled = true;
        while (SDL_PollEvent(&e)) {
#else
        while (SDL_WaitEventTimeout(&e, 16)) {
#endif
#ifdef __ANDROID__
            if (e.type == SDL_EVENT_WILL_ENTER_BACKGROUND && g_vk.dev) vk_pipeline_cache_save();  // (may not come back)
#endif
            if (e.type == SDL_EVENT_QUIT) {
                LOG("window closed\n");
                exit(0);
            }
            extern bool input_user_blocked(const SDL_Event *e);
            if (input_user_blocked(&e)) continue;  // scripted test runs: the real keyboard / mouse / pads do nothing (as metal.m)
#ifdef __ANDROID__
            extern bool touch_controls_event(const SDL_Event *e);  // (ui/touch_controls.mm: fingers, the back button)
            if (touch_controls_event(&e)) continue;
#endif
            if (menu_handle_event(&e)) continue;
            input_handle_event(&e);
        }
        menu_apply_window_requests(window);

#ifdef __ANDROID__
        /*
         * E117 GLES UI submission consumer.
         *
         * The SDL UI thread owns the current GLES context.
         * Limit consumption to avoid starving input processing.
         *
         * GPU execution is not implemented, so every submission
         * is explicitly marked FAILED, never COMPLETED.
         */
        if (nvn_backend_kind() == NVN_BACKEND_GLES_EXPERIMENTAL) {
            for (unsigned gles_job_index = 0;
                 gles_job_index < 8;
                 ++gles_job_index) {
                GlesSubmission gles_job;

                if (!gles_submission_dequeue(&gles_job))
                    break;

                LOG("[gles-backend] E117 UI consumer received sequence=%llu handle=%llx\n",
                    (unsigned long long)gles_job.sequence,
                    (unsigned long long)gles_job.command_handle);

                /*
                 * E123 real guest command decoder.
                 * Synthetic E118 sentinel must never be dereferenced.
                 */
                if (gles_job.command_handle != 0xE118ULL) {
                    extern bool gles_diagnose_command_list(uint64_t handle);

                    LOG("[gles-backend] E123 UI decoding real handle=%llx\n",
                        (unsigned long long)gles_job.command_handle);

                    bool e123_decoded =
                        gles_diagnose_command_list(gles_job.command_handle);

                    LOG("[gles-backend] E123 UI decode finished result=%d\n",
                        e123_decoded ? 1 : 0);

                    /*
                     * E158B: first visible GPU response to a real Zelda
                     * command submission. Diagnostic window clear only.
                     *
                     * No guest texture dereference, no NVN draw execution,
                     * no submission-success acknowledgement.
                     */
                    /* E159B_CAPTURE: Zelda-derived temporary framebuffer.
                     * No real Zelda texture pointer is accessed here.
                     */
                    if (e123_decoded && g_gles_context) {
                        typedef struct {
                            unsigned width, height;
                            unsigned color_format, depth_format;
                        } E159BInfo;
                        extern bool e159b_read_info(E159BInfo *out);

                        E159BInfo info = {0};
                        if (e159b_read_info(&info)) {
                            GLenum cf = 0, df = 0;
                            if (info.color_format == 0x3f)
                                cf = GL_R11F_G11F_B10F;
                            else if (info.color_format == 0x01)
                                cf = GL_R8;
                            else if (info.color_format == 0x25)
                                cf = GL_RGBA8;

                            if (info.depth_format == 0x35)
                                df = GL_DEPTH24_STENCIL8;

                            if (cf && df && info.width > 0 &&
                                info.height > 0 &&
                                info.width <= 2048 && info.height <= 2048) {
                                GLint old_draw = 0, old_read = 0;
                                GLint old_active = 0, old_tex = 0;
                                GLuint color = 0, depth = 0, fbo = 0;

                                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
                                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
                                glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
                                glActiveTexture(GL_TEXTURE0);
                                glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex);

                                glGenTextures(1, &color);
                                glGenTextures(1, &depth);
                                glGenFramebuffers(1, &fbo);

                                if (color && depth && fbo) {
                                    glBindTexture(GL_TEXTURE_2D, color);
                                    glTexStorage2D(GL_TEXTURE_2D, 1, cf,
                                                   info.width, info.height);
                                    glBindTexture(GL_TEXTURE_2D, depth);
                                    glTexStorage2D(GL_TEXTURE_2D, 1, df,
                                                   info.width, info.height);
                                    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                                    glFramebufferTexture2D(
                                        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_TEXTURE_2D, color, 0);
                                    glFramebufferTexture2D(
                                        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                        GL_TEXTURE_2D, depth, 0);
                                    GLenum draw = GL_COLOR_ATTACHMENT0;
                                    glDrawBuffers(1, &draw);

                                    GLenum status = glCheckFramebufferStatus(
                                        GL_FRAMEBUFFER);
                                    GLenum error = glGetError();

                                    /*
                                     * E162: Zelda-derived depth clear.
                                     * Diagnostic framebuffer only.
                                     */
                                    if (status == GL_FRAMEBUFFER_COMPLETE &&
                                        error == GL_NO_ERROR) {
                                        E162DepthClear cmd;
                                        if (e162_get_depth_clear(&cmd) &&
                                            cmd.depth_enable) {
                                            GLint old_viewport[4] = {0};
                                            GLboolean old_depth_mask = GL_FALSE;
                                            GLfloat old_depth_clear = 1.0f;
                                            GLboolean old_scissor =
                                                glIsEnabled(GL_SCISSOR_TEST);

                                            glGetIntegerv(GL_VIEWPORT, old_viewport);
                                            glGetBooleanv(GL_DEPTH_WRITEMASK,
                                                          &old_depth_mask);
                                            glGetFloatv(GL_DEPTH_CLEAR_VALUE,
                                                        &old_depth_clear);

                                            glDisable(GL_SCISSOR_TEST);
                                            glViewport(0, 0, info.width,
                                                       info.height);
                                            glDepthMask(GL_TRUE);
                                            glClearDepthf(cmd.depth_value);
                                            glClear(GL_DEPTH_BUFFER_BIT);

                                            /*
                                             * E162C: GLES-compatible depth
                                             * clear verification.
                                             *
                                             * No unsupported depth readback.
                                             * Query-based verification will
                                             * be implemented separately.
                                             */
                                            GLenum e162_error = glGetError();
                                            bool e162_pass =
                                                e162_error == GL_NO_ERROR;

                                            float actual_depth =
                                                cmd.depth_value;

                                            LOG("[E162C] Depth clear submitted; GPU readback not yet verified\n");
                                            /*
                                             * E163B: read the depth texture
                                             * using GLES 3.2 shader sampling.
                                             * This is a diagnostic operation,
                                             * not real Zelda command replay.
                                             */
                                            {
                                                static const char *vs =
                                                    "#version 320 es\n"
                                                    "void main() {\n"
                                                    "  vec2 p = vec2(\n"
                                                    "    (gl_VertexID == 1) ? 3.0 : -1.0,\n"
                                                    "    (gl_VertexID == 2) ? 3.0 : -1.0);\n"
                                                    "  gl_Position = vec4(p, 0.0, 1.0);\n"
                                                    "}\n";

                                                static const char *fs =
                                                    "#version 320 es\n"
                                                    "precision highp float;\n"
                                                    "uniform highp sampler2D uDepth;\n"
                                                    "layout(location=0) out vec4 outColor;\n"
                                                    "void main() {\n"
                                                    "  float d = texelFetch(uDepth, ivec2(0,0), 0).r;\n"
                                                    "  outColor = vec4(d,d,d,1.0);\n"
                                                    "}\n";

                                                GLuint v = glCreateShader(GL_VERTEX_SHADER);
                                                GLuint f = glCreateShader(GL_FRAGMENT_SHADER);

                                                glShaderSource(v, 1, &vs, NULL);
                                                glCompileShader(v);
                                                glShaderSource(f, 1, &fs, NULL);
                                                glCompileShader(f);

                                                GLint vok = 0, fok = 0;
                                                glGetShaderiv(v, GL_COMPILE_STATUS, &vok);
                                                glGetShaderiv(f, GL_COMPILE_STATUS, &fok);

                                                GLuint program = glCreateProgram();
                                                glAttachShader(program, v);
                                                glAttachShader(program, f);
                                                glLinkProgram(program);

                                                GLint linked = 0;
                                                glGetProgramiv(program, GL_LINK_STATUS, &linked);

                                                LOG("[E163B] shaders vertex=%d fragment=%d linked=%d\n",
                                                    vok, fok, linked);

                                                if (vok && fok && linked) {
                                                    GLint old_program = 0;
                                                    GLint old_vao = 0;
                                                    GLint old_active_tex = 0;
                                                    GLint old_bound_tex = 0;
                                                    GLint e163_viewport[4] = {0};
                                                    GLboolean e163_color_mask[4] = {0};
                                                    GLboolean e163_scissor =
                                                        glIsEnabled(GL_SCISSOR_TEST);
                                                    GLboolean old_depth_test =
                                                        glIsEnabled(GL_DEPTH_TEST);
                                                    /* E163K: preserve diagnostic raster state. */
                                                    GLboolean e163_blend =
                                                        glIsEnabled(GL_BLEND);
                                                    GLboolean e163_stencil =
                                                        glIsEnabled(GL_STENCIL_TEST);
                                                    GLboolean e163_cull =
                                                        glIsEnabled(GL_CULL_FACE);
                                                    GLboolean e163_discard =
                                                        glIsEnabled(GL_RASTERIZER_DISCARD);

                                                    glGetIntegerv(
                                                        GL_VIEWPORT, e163_viewport);
                                                    glGetBooleanv(
                                                        GL_COLOR_WRITEMASK,
                                                        e163_color_mask);

                                                    /* E163E: deterministic diagnostic draw. */
                                                    glDisable(GL_SCISSOR_TEST);
                                                    glDisable(GL_BLEND);
                                                    glDisable(GL_STENCIL_TEST);
                                                    glDisable(GL_CULL_FACE);
                                                    glDisable(GL_RASTERIZER_DISCARD);
                                                    glColorMask(
                                                        GL_TRUE, GL_TRUE,
                                                        GL_TRUE, GL_TRUE);

                                                    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
                                                    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
                                                    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_tex);

                                                    glActiveTexture(GL_TEXTURE0);
                                                    glGetIntegerv(GL_TEXTURE_BINDING_2D,
                                                                  &old_bound_tex);

                                                    GLuint vao = 0;
                                                    glGenVertexArrays(1, &vao);
                                                    glBindVertexArray(vao);

                                                    glUseProgram(program);
                                                    glDisable(GL_DEPTH_TEST);

                                                    glBindTexture(GL_TEXTURE_2D, depth);
                                                    /*
                                                     * E163I: GLES header
                                                     * compatibility.
                                                     *
                                                     * 0x90EA is
                                                     * GL_DEPTH_STENCIL_TEXTURE_MODE.
                                                     */
                                                    glTexParameteri(GL_TEXTURE_2D,
                                                                    0x90EA,
                                                                    GL_DEPTH_COMPONENT);
                                                    glTexParameteri(GL_TEXTURE_2D,
                                                                    GL_TEXTURE_COMPARE_MODE,
                                                                    GL_NONE);
                                                    glTexParameteri(GL_TEXTURE_2D,
                                                                    GL_TEXTURE_MIN_FILTER,
                                                                    GL_NEAREST);
                                                    glTexParameteri(GL_TEXTURE_2D,
                                                                    GL_TEXTURE_MAG_FILTER,
                                                                    GL_NEAREST);

                                                    GLint loc = glGetUniformLocation(
                                                        program, "uDepth");
                                                    glUniform1i(loc, 0);

                                                    /*
                                                     * E163C: sample into a
                                                     * separate color-only FBO.
                                                     * No depth feedback loop.
                                                     */
                                                    GLint old_draw_fbo = 0;
                                                    GLint old_read_fbo = 0;
                                                    glGetIntegerv(
                                                        GL_DRAW_FRAMEBUFFER_BINDING,
                                                        &old_draw_fbo);
                                                    glGetIntegerv(
                                                        GL_READ_FRAMEBUFFER_BINDING,
                                                        &old_read_fbo);

                                                    GLuint test_fbo = 0;
                                                    GLuint test_color = 0;
                                                    glGenFramebuffers(1, &test_fbo);
                                                    glGenTextures(1, &test_color);

                                                    GLubyte pixel[4] = {0};
                                                    GLenum test_error = GL_NO_ERROR;
                                                    GLenum test_status = 0;

                                                    if (test_fbo && test_color) {
                                                        glBindTexture(
                                                            GL_TEXTURE_2D,
                                                            test_color);
                                                        glTexStorage2D(
                                                            GL_TEXTURE_2D, 1,
                                                            GL_RGBA8, 1, 1);

                                                        glBindFramebuffer(
                                                            GL_FRAMEBUFFER,
                                                            test_fbo);
                                                        glFramebufferTexture2D(
                                                            GL_FRAMEBUFFER,
                                                            GL_COLOR_ATTACHMENT0,
                                                            GL_TEXTURE_2D,
                                                            test_color, 0);

                                                        GLenum test_draw =
                                                            GL_COLOR_ATTACHMENT0;
                                                        glDrawBuffers(1, &test_draw);

                                                        test_status =
                                                            glCheckFramebufferStatus(
                                                                GL_FRAMEBUFFER);

                                                        if (test_status ==
                                                            GL_FRAMEBUFFER_COMPLETE) {
                                                            /*
                                                             * E163F: restore
                                                             * the depth sampler
                                                             * binding after
                                                             * test-color setup.
                                                             */
                                                            glActiveTexture(GL_TEXTURE0);
                                                            glBindTexture(GL_TEXTURE_2D,
                                                                          depth);

                                                            glViewport(0, 0, 1, 1);
                                                            glDrawArrays(
                                                                GL_TRIANGLES,
                                                                0, 3);

                                                            glReadBuffer(
                                                                GL_COLOR_ATTACHMENT0);
                                                            glReadPixels(
                                                                0, 0, 1, 1,
                                                                GL_RGBA,
                                                                GL_UNSIGNED_BYTE,
                                                                pixel);

                                                                /*
                                                                 * E164B-R: sample a real depth
                                                                 * transition on the Mali GPU.
                                                                 * Both samples use the E163
                                                                 * shader and independent FBO.
                                                                 */
                                                                if (old_draw_fbo > 0) {
                                                                    const GLfloat sentinel = 0.25f;
                                                                    GLubyte before[4] = {0};
                                                                    GLubyte after[4] = {0};

                                                                    glBindFramebuffer(
                                                                        GL_DRAW_FRAMEBUFFER,
                                                                        (GLuint)old_draw_fbo);

                                                                    /*
                                                                     * E165A: verify that the
                                                                     * framebuffer used by E164B
                                                                     * actually owns the expected
                                                                     * Zelda-derived depth texture.
                                                                     */
                                                                    GLint e165_type = 0;
                                                                    GLint e165_name = 0;
                                                                    glGetFramebufferAttachmentParameteriv(
                                                                        GL_DRAW_FRAMEBUFFER,
                                                                        GL_DEPTH_STENCIL_ATTACHMENT,
                                                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                                        &e165_type);
                                                                    glGetFramebufferAttachmentParameteriv(
                                                                        GL_DRAW_FRAMEBUFFER,
                                                                        GL_DEPTH_STENCIL_ATTACHMENT,
                                                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                                        &e165_name);
                                                                    GLenum e165_error = glGetError();
                                                                    bool e165_pass =
                                                                        e165_error == GL_NO_ERROR &&
                                                                        e165_type == GL_TEXTURE &&
                                                                        e165_name == (GLint)depth;

                                                                    LOG("[E165A] framebuffer=%u depth_texture=%u attachment_type=0x%x attachment_name=%d error=0x%x identity=%s\n",
                                                                        (unsigned)old_draw_fbo,
                                                                        (unsigned)depth,
                                                                        (unsigned)e165_type,
                                                                        (int)e165_name,
                                                                        (unsigned)e165_error,
                                                                        e165_pass ? "PASS" : "FAIL");

                                                                    glDepthMask(GL_TRUE);
                                                                    glClearDepthf(sentinel);
                                                                    glClear(GL_DEPTH_BUFFER_BIT);

                                                                    glBindFramebuffer(
                                                                        GL_DRAW_FRAMEBUFFER, test_fbo);
                                                                    glBindFramebuffer(
                                                                        GL_READ_FRAMEBUFFER, test_fbo);
                                                                    glViewport(0, 0, 1, 1);
                                                                    glActiveTexture(GL_TEXTURE0);
                                                                    glBindTexture(GL_TEXTURE_2D, depth);
                                                                    glDrawArrays(GL_TRIANGLES, 0, 3);
                                                                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                                                                    glReadPixels(
                                                                        0, 0, 1, 1, GL_RGBA,
                                                                        GL_UNSIGNED_BYTE, before);
                                                                    GLenum before_error = glGetError();

                                                                    glBindFramebuffer(
                                                                        GL_DRAW_FRAMEBUFFER,
                                                                        (GLuint)old_draw_fbo);
                                                                    glClearDepthf(cmd.depth_value);
                                                                    glClear(GL_DEPTH_BUFFER_BIT);

                                                                    glBindFramebuffer(
                                                                        GL_DRAW_FRAMEBUFFER, test_fbo);
                                                                    glBindFramebuffer(
                                                                        GL_READ_FRAMEBUFFER, test_fbo);
                                                                    glViewport(0, 0, 1, 1);
                                                                    glActiveTexture(GL_TEXTURE0);
                                                                    glBindTexture(GL_TEXTURE_2D, depth);
                                                                    glDrawArrays(GL_TRIANGLES, 0, 3);
                                                                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                                                                    glReadPixels(
                                                                        0, 0, 1, 1, GL_RGBA,
                                                                        GL_UNSIGNED_BYTE, after);
                                                                    GLenum after_error = glGetError();

                                                                    float before_value =
                                                                        (float)before[0] / 255.0f;
                                                                    float after_value =
                                                                        (float)after[0] / 255.0f;

                                                                    bool pass =
                                                                        before_error == GL_NO_ERROR &&
                                                                        after_error == GL_NO_ERROR &&
                                                                        before_value >= 0.23f &&
                                                                        before_value <= 0.27f &&
                                                                        after_value >= cmd.depth_value - 0.02f &&
                                                                        after_value <= cmd.depth_value + 0.02f &&
                                                                        cmd.depth_value > sentinel + 0.10f;

                                                                    LOG("[E164B] before=%.6f after=%.6f expected=%.6f errors=0x%x,0x%x transition=%s\n",
                                                                        (double)before_value,
                                                                        (double)after_value,
                                                                        (double)cmd.depth_value,
                                                                        (unsigned)before_error,
                                                                        (unsigned)after_error,
                                                                        pass ? "PASS" : "FAIL");
                                                                } else {
                                                                    LOG("[E164B] transition=SKIP unexpected framebuffer binding\n");
                                                                }
                                                        }
                                                    }

                                                    test_error = glGetError();

                                                    /* E163D: normalized 8-bit color readback. */
                                                    float sampled_depth =
                                                        (float)pixel[0] / 255.0f;

                                                    bool depth_pass =
                                                        test_status ==
                                                            GL_FRAMEBUFFER_COMPLETE &&
                                                        test_error == GL_NO_ERROR &&
                                                        sampled_depth >=
                                                            cmd.depth_value - 0.02f &&
                                                        sampled_depth <=
                                                            cmd.depth_value + 0.02f;

                                                    LOG("[E163D] sampled_depth=%.6f requested=%.6f fbo=0x%x error=0x%x pass=%d\n",
                                                        (double)sampled_depth,
                                                        (double)cmd.depth_value,
                                                        (unsigned)test_status,
                                                        (unsigned)test_error,
                                                        depth_pass ? 1 : 0);

                                                    glBindFramebuffer(
                                                        GL_DRAW_FRAMEBUFFER,
                                                        (GLuint)old_draw_fbo);
                                                    glBindFramebuffer(
                                                        GL_READ_FRAMEBUFFER,
                                                        (GLuint)old_read_fbo);

                                                    if (test_fbo)
                                                        glDeleteFramebuffers(
                                                            1, &test_fbo);
                                                    if (test_color)
                                                        glDeleteTextures(
                                                            1, &test_color);

                                                    glBindTexture(GL_TEXTURE_2D,
                                                                  (GLuint)old_bound_tex);
                                                    glActiveTexture((GLenum)old_active_tex);

                                                    glUseProgram((GLuint)old_program);
                                                    glBindVertexArray((GLuint)old_vao);

                                                    glViewport(
                                                        e163_viewport[0],
                                                        e163_viewport[1],
                                                        e163_viewport[2],
                                                        e163_viewport[3]);

                                                    glColorMask(
                                                        e163_color_mask[0],
                                                        e163_color_mask[1],
                                                        e163_color_mask[2],
                                                        e163_color_mask[3]);

                                                    if (e163_scissor)
                                                        glEnable(GL_SCISSOR_TEST);
                                                    else
                                                        glDisable(GL_SCISSOR_TEST);

                                                    if (old_depth_test)
                                                        glEnable(GL_DEPTH_TEST);
                                                    else
                                                        glDisable(GL_DEPTH_TEST);

                                                    if (e163_blend)
                                                        glEnable(GL_BLEND);
                                                    else
                                                        glDisable(GL_BLEND);

                                                    if (e163_stencil)
                                                        glEnable(GL_STENCIL_TEST);
                                                    else
                                                        glDisable(GL_STENCIL_TEST);

                                                    if (e163_cull)
                                                        glEnable(GL_CULL_FACE);
                                                    else
                                                        glDisable(GL_CULL_FACE);

                                                    if (e163_discard)
                                                        glEnable(GL_RASTERIZER_DISCARD);
                                                    else
                                                        glDisable(GL_RASTERIZER_DISCARD);

                                                    glDeleteVertexArrays(1, &vao);
                                                }

                                                glDeleteProgram(program);
                                                glDeleteShader(v);
                                                glDeleteShader(f);
                                            }

                                            LOG("[E162] Zelda depth clear requested=%.3f readback=%.3f error=0x%x pass=%d\n",
                                                (double)cmd.depth_value,
                                                (double)actual_depth,
                                                (unsigned)e162_error,
                                                e162_pass ? 1 : 0);

                                            glViewport(old_viewport[0],
                                                       old_viewport[1],
                                                       old_viewport[2],
                                                       old_viewport[3]);
                                            glDepthMask(old_depth_mask);
                                            glClearDepthf(old_depth_clear);
                                            if (old_scissor)
                                                glEnable(GL_SCISSOR_TEST);
                                            else
                                                glDisable(GL_SCISSOR_TEST);
                                        }
                                    }

                                    /*
                                     * E160: real GPU operation on the
                                     * Zelda-derived temporary framebuffer.
                                     *
                                     * Diagnostic only: this does not execute
                                     * Zelda's recorded clear commands.
                                     */
                                    if (status == GL_FRAMEBUFFER_COMPLETE &&
                                        error == GL_NO_ERROR) {
                                        GLint old_viewport[4] = {0};
                                        GLint old_scissor[4] = {0};
                                        GLboolean old_scissor_enabled =
                                            glIsEnabled(GL_SCISSOR_TEST);
                                        GLboolean old_color_mask[4] = {0};
                                        GLboolean old_depth_mask = GL_FALSE;
                                        GLfloat old_clear_color[4] = {0};
                                        GLfloat old_clear_depth = 1.0f;
                                        GLint old_clear_stencil = 0;

                                        glGetIntegerv(GL_VIEWPORT, old_viewport);
                                        glGetIntegerv(GL_SCISSOR_BOX, old_scissor);
                                        glGetBooleanv(GL_COLOR_WRITEMASK,
                                                      old_color_mask);
                                        glGetBooleanv(GL_DEPTH_WRITEMASK,
                                                      &old_depth_mask);
                                        glGetFloatv(GL_COLOR_CLEAR_VALUE,
                                                    old_clear_color);
                                        glGetFloatv(GL_DEPTH_CLEAR_VALUE,
                                                    &old_clear_depth);
                                        glGetIntegerv(GL_STENCIL_CLEAR_VALUE,
                                                      &old_clear_stencil);

                                        glDisable(GL_SCISSOR_TEST);
                                        glViewport(0, 0, info.width, info.height);
                                        glColorMask(GL_TRUE, GL_TRUE,
                                                    GL_TRUE, GL_TRUE);
                                        glDepthMask(GL_TRUE);
                                        glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
                                        glClearDepthf(1.0f);
                                        glClearStencil(0);
                                        glClear(GL_COLOR_BUFFER_BIT |
                                                GL_DEPTH_BUFFER_BIT |
                                                GL_STENCIL_BUFFER_BIT);

                                        GLfloat pixel[4] = {0};
                                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                                        glReadPixels(0, 0, 1, 1, GL_RGBA,
                                                     GL_FLOAT, pixel);

                                        GLenum e160_error = glGetError();
                                        bool e160_pass =
                                            e160_error == GL_NO_ERROR &&
                                            pixel[0] > 0.20f &&
                                            pixel[0] < 0.30f &&
                                            pixel[1] > 0.45f &&
                                            pixel[1] < 0.55f &&
                                            pixel[2] > 0.70f &&
                                            pixel[2] < 0.80f;

                                        LOG("[E160] Zelda-derived offscreen clear %ux%u readback=(%.3f,%.3f,%.3f) error=0x%x pass=%d\n",
                                            info.width, info.height,
                                            pixel[0], pixel[1], pixel[2],
                                            (unsigned)e160_error,
                                            e160_pass ? 1 : 0);

                                        glViewport(old_viewport[0],
                                                   old_viewport[1],
                                                   old_viewport[2],
                                                   old_viewport[3]);
                                        glScissor(old_scissor[0],
                                                  old_scissor[1],
                                                  old_scissor[2],
                                                  old_scissor[3]);
                                        if (old_scissor_enabled)
                                            glEnable(GL_SCISSOR_TEST);
                                        else
                                            glDisable(GL_SCISSOR_TEST);
                                        glColorMask(old_color_mask[0],
                                                    old_color_mask[1],
                                                    old_color_mask[2],
                                                    old_color_mask[3]);
                                        glDepthMask(old_depth_mask);
                                        glClearColor(old_clear_color[0],
                                                     old_clear_color[1],
                                                     old_clear_color[2],
                                                     old_clear_color[3]);
                                        glClearDepthf(old_clear_depth);
                                        glClearStencil(old_clear_stencil);
                                    }
                                    LOG("[E159B] Zelda-derived FBO %ux%u color=0x%x depth=0x%x status=0x%x error=0x%x complete=%d\n",
                                        info.width, info.height,
                                        info.color_format, info.depth_format,
                                        (unsigned)status, (unsigned)error,
                                        status == GL_FRAMEBUFFER_COMPLETE &&
                                        error == GL_NO_ERROR);
                                } else {
                                    LOG("[E159B] temporary GL allocation failed\n");
                                }

                                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                  (GLuint)old_draw);
                                glBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                  (GLuint)old_read);
                                glBindTexture(GL_TEXTURE_2D, (GLuint)old_tex);
                                glActiveTexture((GLenum)old_active);

                                if (fbo) glDeleteFramebuffers(1, &fbo);
                                if (color) glDeleteTextures(1, &color);
                                if (depth) glDeleteTextures(1, &depth);
                            } else {
                                LOG("[E159B] unsupported sample %ux%u color=0x%x depth=0x%x\n",
                                    info.width, info.height,
                                    info.color_format, info.depth_format);
                            }
                        } else {
                            LOG("[E159B] no published render-target sample\n");
                        }
                    }
                    if (e123_decoded && window && g_gles_context) {
                        int e158_w = 0;
                        int e158_h = 0;

                        SDL_GetWindowSizeInPixels(
                            window, &e158_w, &e158_h);

                        if (e158_w > 0 && e158_h > 0) {
                            glBindFramebuffer(GL_FRAMEBUFFER, 0);
                            glViewport(0, 0, e158_w, e158_h);
                            glDisable(GL_SCISSOR_TEST);
                            glClearColor(0.12f, 0.38f, 0.75f, 1.0f);
                            glClear(GL_COLOR_BUFFER_BIT);

                            GLenum e158_error = glGetError();

                            if (e158_error == GL_NO_ERROR) {
                                SDL_GL_SwapWindow(window);
                                LOG("[E158B] Zelda submission triggered visible GLES diagnostic clear %dx%d\n",
                                    e158_w, e158_h);
                            } else {
                                LOG("[E158B] diagnostic clear failed GL error=0x%x\n",
                                    (unsigned)e158_error);
                            }
                        }
                    }
                }

                if (!gles_submission_finish(gles_job.sequence, false)) {
                    FATAL("[gles-backend] E117 submission completion failed\n");
                }

                /*
                 * E120 synthetic submission retirement.
                 * Only the E118 sentinel is retired here.
                 * Future real submissions remain owned by their producer.
                 */
                if (gles_job.command_handle == 0xE118ULL) {
                    GlesSubmissionState e120_state = GLES_SUBMISSION_PENDING;

                    if (!gles_submission_wait_completion(
                            gles_job.sequence, 0, &e120_state)) {
                        FATAL("[gles-backend] E120 completion query failed\n");
                    }

                    if (e120_state != GLES_SUBMISSION_FAILED) {
                        FATAL("[gles-backend] E120 unexpected completion state=%d\n",
                              (int)e120_state);
                    }

                    if (!gles_submission_retire(gles_job.sequence)) {
                        FATAL("[gles-backend] E120 retirement failed\n");
                    }

                    LOG("[gles-backend] E120 synthetic submission verified and retired sequence=%llu\n",
                        (unsigned long long)gles_job.sequence);
                }
            }
        }
#endif
#ifdef __ANDROID__
        {   // the pipeline cache to disk now and then while it grows: an app is killed rather than exited, so
            // quick_exit_handler never saves it (every start compiled every shader again: missing objects at first)
            static uint64_t last;
            static size_t saved;
            uint64_t now = SDL_GetTicks();
            if (g_vk.dev && now - last > 30000) {
                last = now;
                size_t sz = 0;
                if (vkGetPipelineCacheData(g_vk.dev, g_vk.pcache, &sz, NULL) == VK_SUCCESS && sz > saved + 65536) {
                    vk_pipeline_cache_save();
                    saved = sz;
                    LOG("[vk] pipeline cache saved (%zu KB)\n", sz >> 10);
                }
            }
        }
#endif
    }
}
