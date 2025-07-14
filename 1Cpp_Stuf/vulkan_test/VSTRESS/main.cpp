#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <chrono>

#include "shader_vert.h"
#include "shader_frag.h"

const int MAX_FRAMES_IN_FLIGHT = 2;
const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

int main() {
    // --- 1. Init GLFW and Vulkan Instance ---
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "GPU Graphics Stress Test", nullptr, nullptr);

    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    VkInstance instance;
    VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.pApplicationName="Graphics Stress Test"; appInfo.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instInfo{}; instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; instInfo.pApplicationInfo=&appInfo; instInfo.enabledExtensionCount=count; instInfo.ppEnabledExtensionNames=extensions;
    vkCreateInstance(&instInfo, nullptr, &instance);

    // --- 2. Surface, Device, Queues ---
    VkSurfaceKHR surface;
    glfwCreateWindowSurface(instance, window, nullptr, &surface);

    VkPhysicalDevice physicalDevice;
    vkEnumeratePhysicalDevices(instance, &count, nullptr); std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data()); physicalDevice=devices[0];

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr); std::vector<VkQueueFamilyProperties> queueFamilies(count); vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queueFamilies.data());
    uint32_t graphicsFamily = -1;
    for(uint32_t i=0; i<queueFamilies.size(); ++i) if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsFamily=i; break; }

    VkDevice device;
    float qp=1.0f; VkDeviceQueueCreateInfo qcInfo{}; qcInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcInfo.queueFamilyIndex = graphicsFamily; qcInfo.queueCount=1; qcInfo.pQueuePriorities=&qp;
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dInfo{}; dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dInfo.queueCreateInfoCount=1; dInfo.pQueueCreateInfos = &qcInfo; dInfo.enabledExtensionCount=1; dInfo.ppEnabledExtensionNames=deviceExtensions;
    vkCreateDevice(physicalDevice, &dInfo, nullptr, &device);

    VkQueue queue;
    vkGetDeviceQueue(device, graphicsFamily, 0, &queue);

    // --- 3. Swapchain, Image Views, Render Pass ---
    VkSwapchainKHR swapchain;
    VkSwapchainCreateInfoKHR scInfo{}; scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; scInfo.surface = surface; scInfo.minImageCount = 2; scInfo.imageFormat=VK_FORMAT_B8G8R8A8_SRGB; scInfo.imageColorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; scInfo.imageExtent={WIDTH,HEIGHT}; scInfo.imageArrayLayers=1; scInfo.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; scInfo.presentMode=VK_PRESENT_MODE_IMMEDIATE_KHR; // Uncapped FPS
    vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain);

    vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr); std::vector<VkImage> swapchainImages(count); vkGetSwapchainImagesKHR(device, swapchain, &count, swapchainImages.data());
    std::vector<VkImageView> swapchainImageViews(count);
    for(size_t i=0; i<count; ++i) {
        VkImageViewCreateInfo ivInfo{}; ivInfo.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; ivInfo.image=swapchainImages[i]; ivInfo.viewType=VK_IMAGE_VIEW_TYPE_2D; ivInfo.format=scInfo.imageFormat; ivInfo.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; ivInfo.subresourceRange.levelCount=1; ivInfo.subresourceRange.layerCount=1;
        vkCreateImageView(device, &ivInfo, nullptr, &swapchainImageViews[i]);
    }

    VkAttachmentDescription colorAttachment{}; colorAttachment.format=scInfo.imageFormat; colorAttachment.samples=VK_SAMPLE_COUNT_1_BIT; colorAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; colorAttachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE; colorAttachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; colorAttachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE; colorAttachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; colorAttachment.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorAttachmentRef{}; colorAttachmentRef.attachment=0; colorAttachmentRef.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount=1; subpass.pColorAttachments=&colorAttachmentRef;
    VkRenderPass renderPass;
    VkRenderPassCreateInfo rpInfo{}; rpInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; rpInfo.attachmentCount=1; rpInfo.pAttachments=&colorAttachment; rpInfo.subpassCount=1; rpInfo.pSubpasses=&subpass;
    vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass);

    // --- 4. Graphics Pipeline and Framebuffers ---
    VkShaderModule vertModule, fragModule;
    VkShaderModuleCreateInfo smInfo{}; smInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smInfo.codeSize=sizeof(shader_vert_spv); smInfo.pCode=reinterpret_cast<const uint32_t*>(shader_vert_spv); vkCreateShaderModule(device, &smInfo, nullptr, &vertModule); smInfo.codeSize=sizeof(shader_frag_spv); smInfo.pCode=reinterpret_cast<const uint32_t*>(shader_frag_spv); vkCreateShaderModule(device, &smInfo, nullptr, &fragModule);
    VkPipelineShaderStageCreateInfo stages[2]{}; stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertModule; stages[0].pName="main"; stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=fragModule; stages[1].pName="main";
    VkPushConstantRange pushConstantRange{}; pushConstantRange.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT; pushConstantRange.size=sizeof(float);
    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo plInfo{}; plInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; plInfo.pushConstantRangeCount=1; plInfo.pPushConstantRanges=&pushConstantRange;
    vkCreatePipelineLayout(device, &plInfo, nullptr, &pipelineLayout);
    VkPipeline pipeline;
    VkGraphicsPipelineCreateInfo pInfo{}; pInfo.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; pInfo.stageCount=2; pInfo.pStages=stages;
    VkPipelineVertexInputStateCreateInfo vi{}; vi.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO; pInfo.pVertexInputState=&vi;
    VkPipelineInputAssemblyStateCreateInfo ia{}; ia.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; pInfo.pInputAssemblyState=&ia;
    VkViewport viewport{}; viewport.width=WIDTH; viewport.height=HEIGHT; viewport.maxDepth=1.0f; VkRect2D scissor{}; scissor.extent={WIDTH,HEIGHT};
    VkPipelineViewportStateCreateInfo vp{}; vp.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vp.viewportCount=1; vp.pViewports=&viewport; vp.scissorCount=1; vp.pScissors=&scissor; pInfo.pViewportState=&vp;
    VkPipelineRasterizationStateCreateInfo rs{}; rs.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; rs.lineWidth=1.0f; pInfo.pRasterizationState=&rs;
    VkPipelineMultisampleStateCreateInfo ms{}; ms.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT; pInfo.pMultisampleState=&ms;
    VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{}; cb.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cb.attachmentCount=1; cb.pAttachments=&cba; pInfo.pColorBlendState=&cb;
    pInfo.layout=pipelineLayout; pInfo.renderPass=renderPass;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pInfo, nullptr, &pipeline);

    std::vector<VkFramebuffer> framebuffers(count);
    for(size_t i=0; i<count; ++i) {
        VkFramebufferCreateInfo fbInfo{}; fbInfo.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; fbInfo.renderPass=renderPass; fbInfo.attachmentCount=1; fbInfo.pAttachments=&swapchainImageViews[i]; fbInfo.width=WIDTH; fbInfo.height=HEIGHT; fbInfo.layers=1;
        vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]);
    }

    // --- 5. Command Pool, Buffers, and Synchronization ---
    VkCommandPool commandPool;
    VkCommandPoolCreateInfo cpInfo{}; cpInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpInfo.queueFamilyIndex=graphicsFamily;
    vkCreateCommandPool(device, &cpInfo, nullptr, &commandPool);

    std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cbAllocInfo{}; cbAllocInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbAllocInfo.commandPool=commandPool; cbAllocInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    vkAllocateCommandBuffers(device, &cbAllocInfo, commandBuffers.data());

    std::vector<VkSemaphore> imageAvailableSemaphores(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinishedSemaphores(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo semInfo{}; semInfo.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{}; fenceInfo.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fenceInfo.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]);
        vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
    }

    // --- 6. The Complete Render Loop ---
    std::cout << "Starting Graphics Stress Test. Monitor GPU Power (Watts). Close window to exit." << std::endl;
    int currentFrame = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        VkCommandBuffer cb = commandBuffers[currentFrame];
        VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cb, &beginInfo);

        VkRenderPassBeginInfo rpbInfo{}; rpbInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; rpbInfo.renderPass=renderPass; rpbInfo.framebuffer=framebuffers[imageIndex]; rpbInfo.renderArea.extent={WIDTH,HEIGHT};
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        rpbInfo.clearValueCount=1; rpbInfo.pClearValues=&clearColor;

        vkCmdBeginRenderPass(cb, &rpbInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &time);

        vkCmdDraw(cb, 3, 1, 0, 0);
        vkCmdEndRenderPass(cb);
        vkEndCommandBuffer(cb);

        VkSubmitInfo submitInfo{}; submitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount=1;
        submitInfo.pCommandBuffers = &cb;
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]);

        VkPresentInfoKHR presentInfo{}; presentInfo.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount=1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = {swapchain};
        presentInfo.swapchainCount=1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(queue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);

    // --- 7. Cleanup ---
    // Destroy all created Vulkan objects...
    return 0;
}
