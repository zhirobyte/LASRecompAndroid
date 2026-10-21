/* H1-13D: isolated Vulkan 1.1 offscreen triangle probe.
 * Included from device.c under __ANDROID__, after triangle SPIR-V headers.
 * Return VK_TRUE ONLY if submitted work may still be executing: caller must
 * retain device, instance, Vulkan library and all resources until process exit.
 */
#ifndef H1_GRAPHICS_PROBE_H
#define H1_GRAPHICS_PROBE_H
static VkBool32 h1_probe_graphics_11(VkInstance instance, VkPhysicalDevice physical,
    VkDevice device, uint32_t family, PFN_vkGetInstanceProcAddr gipa,
    PFN_vkGetDeviceProcAddr gdpa)
{
#define H1G_LOAD(type, name) type name = (type)gdpa(device, #name)
    H1G_LOAD(PFN_vkCreateImage, vkCreateImage);
    H1G_LOAD(PFN_vkDestroyImage, vkDestroyImage);
    H1G_LOAD(PFN_vkGetImageMemoryRequirements, vkGetImageMemoryRequirements);
    H1G_LOAD(PFN_vkCreateImageView, vkCreateImageView);
    H1G_LOAD(PFN_vkDestroyImageView, vkDestroyImageView);
    H1G_LOAD(PFN_vkCreateBuffer, vkCreateBuffer);
    H1G_LOAD(PFN_vkDestroyBuffer, vkDestroyBuffer);
    H1G_LOAD(PFN_vkGetBufferMemoryRequirements, vkGetBufferMemoryRequirements);
    H1G_LOAD(PFN_vkAllocateMemory, vkAllocateMemory);
    H1G_LOAD(PFN_vkFreeMemory, vkFreeMemory);
    H1G_LOAD(PFN_vkBindImageMemory, vkBindImageMemory);
    H1G_LOAD(PFN_vkBindBufferMemory, vkBindBufferMemory);
    H1G_LOAD(PFN_vkMapMemory, vkMapMemory);
    H1G_LOAD(PFN_vkUnmapMemory, vkUnmapMemory);
    H1G_LOAD(PFN_vkInvalidateMappedMemoryRanges, vkInvalidateMappedMemoryRanges);
    H1G_LOAD(PFN_vkCreateRenderPass, vkCreateRenderPass);
    H1G_LOAD(PFN_vkDestroyRenderPass, vkDestroyRenderPass);
    H1G_LOAD(PFN_vkCreateFramebuffer, vkCreateFramebuffer);
    H1G_LOAD(PFN_vkDestroyFramebuffer, vkDestroyFramebuffer);
    H1G_LOAD(PFN_vkCreateShaderModule, vkCreateShaderModule);
    H1G_LOAD(PFN_vkDestroyShaderModule, vkDestroyShaderModule);
    H1G_LOAD(PFN_vkCreatePipelineLayout, vkCreatePipelineLayout);
    H1G_LOAD(PFN_vkDestroyPipelineLayout, vkDestroyPipelineLayout);
    H1G_LOAD(PFN_vkCreateGraphicsPipelines, vkCreateGraphicsPipelines);
    H1G_LOAD(PFN_vkDestroyPipeline, vkDestroyPipeline);
    H1G_LOAD(PFN_vkCreateCommandPool, vkCreateCommandPool);
    H1G_LOAD(PFN_vkDestroyCommandPool, vkDestroyCommandPool);
    H1G_LOAD(PFN_vkAllocateCommandBuffers, vkAllocateCommandBuffers);
    H1G_LOAD(PFN_vkBeginCommandBuffer, vkBeginCommandBuffer);
    H1G_LOAD(PFN_vkEndCommandBuffer, vkEndCommandBuffer);
    H1G_LOAD(PFN_vkCmdBeginRenderPass, vkCmdBeginRenderPass);
    H1G_LOAD(PFN_vkCmdEndRenderPass, vkCmdEndRenderPass);
    H1G_LOAD(PFN_vkCmdBindPipeline, vkCmdBindPipeline);
    H1G_LOAD(PFN_vkCmdDraw, vkCmdDraw);
    H1G_LOAD(PFN_vkCmdPipelineBarrier, vkCmdPipelineBarrier);
    H1G_LOAD(PFN_vkCmdCopyImageToBuffer, vkCmdCopyImageToBuffer);
    H1G_LOAD(PFN_vkGetDeviceQueue, vkGetDeviceQueue);
    H1G_LOAD(PFN_vkQueueSubmit, vkQueueSubmit);
    H1G_LOAD(PFN_vkCreateFence, vkCreateFence);
    H1G_LOAD(PFN_vkDestroyFence, vkDestroyFence);
    H1G_LOAD(PFN_vkWaitForFences, vkWaitForFences);
#undef H1G_LOAD
    PFN_vkGetPhysicalDeviceMemoryProperties getMem =
        (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(instance, "vkGetPhysicalDeviceMemoryProperties");
    PFN_vkGetPhysicalDeviceFormatProperties getFormat =
        (PFN_vkGetPhysicalDeviceFormatProperties)gipa(instance, "vkGetPhysicalDeviceFormatProperties");
    PFN_vkGetPhysicalDeviceImageFormatProperties getImageFormat =
        (PFN_vkGetPhysicalDeviceImageFormatProperties)gipa(instance, "vkGetPhysicalDeviceImageFormatProperties");
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueues =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)gipa(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    if (!getMem || !getFormat || !getImageFormat || !getQueues ||
        !vkCreateImage || !vkDestroyImage || !vkGetImageMemoryRequirements ||
        !vkCreateImageView || !vkDestroyImageView || !vkCreateBuffer ||
        !vkDestroyBuffer || !vkGetBufferMemoryRequirements || !vkAllocateMemory ||
        !vkFreeMemory || !vkBindImageMemory || !vkBindBufferMemory ||
        !vkMapMemory || !vkUnmapMemory || !vkInvalidateMappedMemoryRanges ||
        !vkCreateRenderPass || !vkDestroyRenderPass || !vkCreateFramebuffer ||
        !vkDestroyFramebuffer || !vkCreateShaderModule || !vkDestroyShaderModule ||
        !vkCreatePipelineLayout || !vkDestroyPipelineLayout ||
        !vkCreateGraphicsPipelines || !vkDestroyPipeline || !vkCreateCommandPool ||
        !vkDestroyCommandPool || !vkAllocateCommandBuffers || !vkBeginCommandBuffer ||
        !vkEndCommandBuffer || !vkCmdBeginRenderPass || !vkCmdEndRenderPass ||
        !vkCmdBindPipeline || !vkCmdDraw || !vkCmdPipelineBarrier ||
        !vkCmdCopyImageToBuffer || !vkGetDeviceQueue || !vkQueueSubmit ||
        !vkCreateFence || !vkDestroyFence || !vkWaitForFences) {
        LOG("[h1-vulkan-graphics] missing_entry_point=1\n");
        return VK_FALSE;
    }
    LOG("[h1-vulkan-graphics] starting\n");
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkBool32 pending = VK_FALSE, coherent = VK_FALSE;
    VkResult r;
    VkPhysicalDeviceMemoryProperties mp;
    getMem(physical, &mp);
    VkFormatProperties fp;
    getFormat(physical, VK_FORMAT_R8G8B8A8_UNORM, &fp);
    VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                  VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    VkImageFormatProperties ifp;
    r = getImageFormat(physical, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &ifp);
    if (r != VK_SUCCESS || (fp.optimalTilingFeatures & needed) != needed ||
        ifp.maxExtent.width < 256 || ifp.maxExtent.height < 256 ||
        !(ifp.sampleCounts & VK_SAMPLE_COUNT_1_BIT)) {
        LOG("[h1-vulkan-graphics] unsupported_format=%d\n", r);
        goto cleanup;
    }
    uint32_t qc = 0;
    getQueues(physical, &qc, NULL);
    if (family >= qc || qc == 0 || qc > 64) goto cleanup;
    VkQueueFamilyProperties qp[64];
    getQueues(physical, &qc, qp);
    if (!(qp[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !qp[family].queueCount) {
        LOG("[h1-vulkan-graphics] no_graphics_queue=1\n");
        goto cleanup;
    }
    VkImageCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {256, 256, 1}, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    r = vkCreateImage(device, &ici, NULL, &image);
    LOG("[h1-vulkan-graphics] image_result=%d\n", r);
    if (r != VK_SUCCESS) goto cleanup;
    VkMemoryRequirements imr;
    vkGetImageMemoryRequirements(device, image, &imr);
    uint32_t mi = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((imr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            mi = i; break;
        }
    if (mi == UINT32_MAX)
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if (imr.memoryTypeBits & (1u << i)) {mi = i; break;}
    if (mi == UINT32_MAX) goto cleanup;
    VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = imr.size, .memoryTypeIndex = mi};
    r = vkAllocateMemory(device, &mai, NULL, &imageMemory);
    if (r != VK_SUCCESS) goto cleanup;
    r = vkBindImageMemory(device, image, imageMemory, 0);
    if (r != VK_SUCCESS) goto cleanup;
    VkImageViewCreateInfo ivci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    r = vkCreateImageView(device, &ivci, NULL, &view);
    if (r != VK_SUCCESS) goto cleanup;
    VkBufferCreateInfo bci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 256u * 256u * 4u, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    r = vkCreateBuffer(device, &bci, NULL, &readback);
    if (r != VK_SUCCESS) goto cleanup;
    VkMemoryRequirements bmr;
    vkGetBufferMemoryRequirements(device, readback, &bmr);
    mi = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bmr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            mi = i; coherent = VK_TRUE; break;
        }
    if (mi == UINT32_MAX)
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((bmr.memoryTypeBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                mi = i; break;
            }
    if (mi == UINT32_MAX) goto cleanup;
    mai.allocationSize = bmr.size;
    mai.memoryTypeIndex = mi;
    r = vkAllocateMemory(device, &mai, NULL, &readbackMemory);
    if (r != VK_SUCCESS) goto cleanup;
    r = vkBindBufferMemory(device, readback, readbackMemory, 0);
    if (r != VK_SUCCESS) goto cleanup;
    VkAttachmentDescription attachment = {.format = VK_FORMAT_R8G8B8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference attachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1, .pColorAttachments = &attachmentRef};
    VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &attachment,
        .subpassCount = 1, .pSubpasses = &subpass};
    r = vkCreateRenderPass(device, &rpci, NULL, &renderPass);
    LOG("[h1-vulkan-graphics] renderpass_result=%d\n", r);
    if (r != VK_SUCCESS) goto cleanup;
    VkFramebufferCreateInfo fbci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass, .attachmentCount = 1, .pAttachments = &view,
        .width = 256, .height = 256, .layers = 1};
    r = vkCreateFramebuffer(device, &fbci, NULL, &framebuffer);
    if (r != VK_SUCCESS) goto cleanup;
    VkShaderModuleCreateInfo smci = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(h1_triangle_vert_spirv), .pCode = h1_triangle_vert_spirv};
    r = vkCreateShaderModule(device, &smci, NULL, &vert);
    if (r != VK_SUCCESS) goto cleanup;
    smci.codeSize = sizeof(h1_triangle_frag_spirv);
    smci.pCode = h1_triangle_frag_spirv;
    r = vkCreateShaderModule(device, &smci, NULL, &frag);
    if (r != VK_SUCCESS) goto cleanup;
    VkPipelineLayoutCreateInfo plci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    r = vkCreatePipelineLayout(device, &plci, NULL, &layout);
    if (r != VK_SUCCESS) goto cleanup;
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"}};
    VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport viewport = {0, 0, 256, 256, 0, 1};
    VkRect2D scissor = {{0, 0}, {256, 256}};
    VkPipelineViewportStateCreateInfo vp = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};
    VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState cbatt = {.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &cbatt};
    VkGraphicsPipelineCreateInfo gpci = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages, .pVertexInputState = &vi,
        .pInputAssemblyState = &ia, .pViewportState = &vp,
        .pRasterizationState = &rs, .pMultisampleState = &ms,
        .pColorBlendState = &cb, .layout = layout, .renderPass = renderPass,
        .subpass = 0};
    r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline);
    LOG("[h1-vulkan-graphics] pipeline_result=%d\n", r);
    if (r != VK_SUCCESS) goto cleanup;
    VkCommandPoolCreateInfo cpci = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = family};
    r = vkCreateCommandPool(device, &cpci, NULL, &pool);
    if (r != VK_SUCCESS) goto cleanup;
    VkCommandBufferAllocateInfo cbai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    r = vkAllocateCommandBuffers(device, &cbai, &cmd);
    if (r != VK_SUCCESS) goto cleanup;
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    r = vkBeginCommandBuffer(cmd, &begin);
    if (r != VK_SUCCESS) goto cleanup;
    VkClearValue clear = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpbegin = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass, .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {256, 256}}, .clearValueCount = 1, .pClearValues = &clear};
    vkCmdBeginRenderPass(cmd, &rpbegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    // H1-15P: exercise the non-indexed NVN draw argument layout through
    // the verified Vulkan 1.1 render pass. This is a SYNTHETIC CmdRec,
    // not a command captured from Zelda or the production NVN executor.
    CmdRec h1_draw = {0};
    h1_draw.op = CMD_DRAW_ARRAYS;
    h1_draw.size = sizeof(CmdRec);
    h1_draw.x[0] = 4;  // NVN primitive: triangles (validated below)
    h1_draw.x[1] = 0;  // first vertex
    h1_draw.x[2] = 3;  // vertex count
    h1_draw.x[3] = 0;  // base instance
    h1_draw.x[4] = 1;  // instance count
    if (h1_draw.op != CMD_DRAW_ARRAYS || h1_draw.x[0] != 4 ||
        h1_draw.x[2] == 0 || h1_draw.x[2] > 3 ||
        h1_draw.x[1] > 2 || h1_draw.x[3] != 0 ||
        h1_draw.x[4] != 1) {
        LOG("[h1-15p] synthetic_nvn_draw_rejected=1\n");
        vkCmdEndRenderPass(cmd);
        goto cleanup;
    }
    LOG("[h1-15p] synthetic_nvn_draw op=%u first=%u count=%u instances=%u\n",
        h1_draw.op, (uint32_t)h1_draw.x[1], (uint32_t)h1_draw.x[2],
        (uint32_t)h1_draw.x[4]);
    vkCmdDraw(cmd, (uint32_t)h1_draw.x[2], (uint32_t)h1_draw.x[4],
              (uint32_t)h1_draw.x[1], (uint32_t)h1_draw.x[3]);
    vkCmdEndRenderPass(cmd);
    VkImageMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    VkBufferImageCopy copy = {.bufferOffset = 0, .bufferRowLength = 0,
        .bufferImageHeight = 0, .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {256, 256, 1}};
    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        readback, 1, &copy);
    VkBufferMemoryBarrier hostBarrier = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = readback, .offset = 0, .size = VK_WHOLE_SIZE};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &hostBarrier, 0, NULL);
    r = vkEndCommandBuffer(cmd);
    if (r != VK_SUCCESS) goto cleanup;
    LOG("[h1-vulkan-graphics] draw_recorded=1\n");
    VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    r = vkCreateFence(device, &fci, NULL, &fence);
    if (r != VK_SUCCESS) goto cleanup;
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, family, 0, &queue);
    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &cmd};
    r = vkQueueSubmit(queue, 1, &si, fence);
    LOG("[h1-vulkan-graphics] submit_result=%d\n", r);
    if (r != VK_SUCCESS) goto cleanup;
    pending = VK_TRUE;
    r = vkWaitForFences(device, 1, &fence, VK_TRUE, 3000000000ULL);
    LOG("[h1-vulkan-graphics] wait_result=%d\n", r);
    if (r != VK_SUCCESS) goto cleanup;
    pending = VK_FALSE;
    void *mapped = NULL;
    r = vkMapMemory(device, readbackMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
    LOG("[h1-vulkan-graphics] map_result=%d\n", r);
    if (r != VK_SUCCESS) goto cleanup;
    if (!coherent) {
        VkMappedMemoryRange range = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = readbackMemory, .offset = 0, .size = VK_WHOLE_SIZE};
        r = vkInvalidateMappedMemoryRanges(device, 1, &range);
    }
    if (r == VK_SUCCESS) {
        const uint8_t *pixels = (const uint8_t *)mapped;
        const uint8_t *center = pixels + ((128u * 256u + 128u) * 4u);
        const uint8_t *corner = pixels + ((8u * 256u + 8u) * 4u);
        uint32_t centerRGB = (uint32_t)center[0] + center[1] + center[2];
        uint32_t cornerRGB = (uint32_t)corner[0] + corner[1] + corner[2];
        VkBool32 pass = centerRGB > 40 && cornerRGB < 12 &&
            center[3] >= 250 && corner[3] >= 250;
        LOG("[h1-vulkan-graphics] center_rgba=%u,%u,%u,%u corner_rgba=%u,%u,%u,%u pass=%u\n",
            center[0], center[1], center[2], center[3],
            corner[0], corner[1], corner[2], corner[3], pass);
    } else {
        LOG("[h1-vulkan-graphics] invalidate_result=%d\n", r);
    }
    vkUnmapMemory(device, readbackMemory);
cleanup:
    if (pending) {
        LOG("[h1-vulkan-graphics] cleanup_deferred_gpu_pending=1\n");
        return VK_TRUE;
    }
    if (fence) vkDestroyFence(device, fence, NULL);
    if (pool) vkDestroyCommandPool(device, pool, NULL);
    if (pipeline) vkDestroyPipeline(device, pipeline, NULL);
    if (layout) vkDestroyPipelineLayout(device, layout, NULL);
    if (vert) vkDestroyShaderModule(device, vert, NULL);
    if (frag) vkDestroyShaderModule(device, frag, NULL);
    if (framebuffer) vkDestroyFramebuffer(device, framebuffer, NULL);
    if (renderPass) vkDestroyRenderPass(device, renderPass, NULL);
    if (view) vkDestroyImageView(device, view, NULL);
    if (readback) vkDestroyBuffer(device, readback, NULL);
    if (image) vkDestroyImage(device, image, NULL);
    if (readbackMemory) vkFreeMemory(device, readbackMemory, NULL);
    if (imageMemory) vkFreeMemory(device, imageMemory, NULL);
    LOG("[h1-vulkan-graphics] cleanup_complete=1\n");
    return VK_FALSE;
}
#endif /* H1_GRAPHICS_PROBE_H */
