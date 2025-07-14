#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <string> // *** NEW: Needed for std::to_string ***

// Embed all three shaders
#include "noise_generator.h"
#include "shader_vert.h"
#include "shader_frag.h"

// --- Configuration ---
const int MAX_FRAMES_IN_FLIGHT = 2;
// These are now just initial values, they will be updated by the window
uint32_t WIDTH = 1280;
uint32_t HEIGHT = 720;
const uint32_t NOISE_DIM = 1024;
const int OVERDRAW_LAYERS = 8;

// --- Global state for resize handling ---
bool framebufferResized = false;

// --- Forward declaration of the main application struct ---
struct VulkanApp;

// --- GLFW callback function for window resize ---
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    framebufferResized = true;
}

// --- START OF VULKAN HELPER FUNCTIONS (These are complete and correct) ---
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties; vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) { return i; }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{}; imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; imageInfo.imageType = VK_IMAGE_TYPE_2D; imageInfo.extent.width = width; imageInfo.extent.height = height; imageInfo.extent.depth = 1; imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1; imageInfo.format = format; imageInfo.tiling = tiling; imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; imageInfo.usage = usage; imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) throw std::runtime_error("failed to create image!");
    VkMemoryRequirements memRequirements; vkGetImageMemoryRequirements(device, image, &memRequirements);
    VkMemoryAllocateInfo allocInfo{}; allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; allocInfo.allocationSize = memRequirements.size; allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) throw std::runtime_error("failed to allocate image memory!");
    vkBindImageMemory(device, image, imageMemory, 0);
}
VkImageView createImageView(VkDevice device, VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo{}; viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; viewInfo.image = image; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = format; viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) throw std::runtime_error("failed to create texture image view!");
    return imageView;
}
VkResult createShaderModule(VkDevice device, const unsigned char* code, size_t size, VkShaderModule* shaderModule) {
    VkShaderModuleCreateInfo createInfo{}; createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; createInfo.codeSize = size; createInfo.pCode = reinterpret_cast<const uint32_t*>(code);
    return vkCreateShaderModule(device, &createInfo, nullptr, shaderModule);
}
VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{}; allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocInfo.commandPool = commandPool; allocInfo.commandBufferCount = 1; VkCommandBuffer commandBuffer; vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}
void endSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer); VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer; vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE); vkQueueWaitIdle(queue); vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);
    VkImageMemoryBarrier barrier{}; barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; barrier.oldLayout = oldLayout; barrier.newLayout = newLayout; barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.image = image; barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
    VkPipelineStageFlags sourceStage, destinationStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) { barrier.srcAccessMask = 0; barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
    else { throw std::invalid_argument("unsupported layout transition!"); }
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(device, queue, commandPool, commandBuffer);
}
// --- END OF VULKAN HELPER FUNCTIONS ---

struct VulkanApp { GLFWwindow* window; VkInstance instance; VkSurfaceKHR surface; VkPhysicalDevice physicalDevice; VkDevice device; VkQueue queue; uint32_t graphicsFamily; VkSwapchainKHR swapchain; std::vector<VkImage> swapchainImages; std::vector<VkImageView> swapchainImageViews; std::vector<VkFramebuffer> framebuffers; VkFormat swapchainImageFormat; VkExtent2D swapchainExtent; VkRenderPass renderPass; VkDescriptorSetLayout graphicsSetLayout; VkPipelineLayout graphicsPipelineLayout; VkPipeline graphicsPipeline; VkCommandPool commandPool; std::vector<VkCommandBuffer> commandBuffers; std::vector<VkSemaphore> imageAvailableSemaphores; std::vector<VkSemaphore> renderFinishedSemaphores; std::vector<VkFence> inFlightFences; VkImage noiseImage; VkDeviceMemory noiseImageMemory; VkImageView noiseImageView; VkSampler sampler; VkDescriptorPool descriptorPool; VkDescriptorSet graphicsDescriptorSet; };

void cleanupSwapChain(VulkanApp& app) {
    for (auto framebuffer : app.framebuffers) { vkDestroyFramebuffer(app.device, framebuffer, nullptr); }
    for (auto imageView : app.swapchainImageViews) { vkDestroyImageView(app.device, imageView, nullptr); }
    vkDestroySwapchainKHR(app.device, app.swapchain, nullptr);
}
void createSwapChain(VulkanApp& app) {
    int width, height; glfwGetFramebufferSize(app.window, &width, &height);
    app.swapchainExtent = { (uint32_t)width, (uint32_t)height }; app.swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkSwapchainCreateInfoKHR createInfo{}; createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; createInfo.surface = app.surface; createInfo.minImageCount = 2; createInfo.imageFormat = app.swapchainImageFormat; createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; createInfo.imageExtent = app.swapchainExtent; createInfo.imageArrayLayers = 1; createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; createInfo.clipped = VK_TRUE; createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(app.device, &createInfo, nullptr, &app.swapchain) != VK_SUCCESS) throw std::runtime_error("failed to create swapchain!");
    uint32_t imageCount; vkGetSwapchainImagesKHR(app.device, app.swapchain, &imageCount, nullptr); app.swapchainImages.resize(imageCount); vkGetSwapchainImagesKHR(app.device, app.swapchain, &imageCount, app.swapchainImages.data());
    app.swapchainImageViews.resize(imageCount); for (size_t i = 0; i < imageCount; i++) { app.swapchainImageViews[i] = createImageView(app.device, app.swapchainImages[i], app.swapchainImageFormat); }
}
void createFramebuffers(VulkanApp& app) {
    app.framebuffers.resize(app.swapchainImageViews.size());
    for (size_t i = 0; i < app.swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo{}; fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; fbInfo.renderPass = app.renderPass; fbInfo.attachmentCount = 1; fbInfo.pAttachments = &app.swapchainImageViews[i]; fbInfo.width = app.swapchainExtent.width; fbInfo.height = app.swapchainExtent.height; fbInfo.layers = 1;
        if (vkCreateFramebuffer(app.device, &fbInfo, nullptr, &app.framebuffers[i]) != VK_SUCCESS) throw std::runtime_error("failed to create framebuffer!");
    }
}
void recreateSwapChain(VulkanApp& app) {
    int width = 0, height = 0; glfwGetFramebufferSize(app.window, &width, &height);
    while (width == 0 || height == 0) { glfwGetFramebufferSize(app.window, &width, &height); glfwWaitEvents(); }
    vkDeviceWaitIdle(app.device);
    cleanupSwapChain(app);
    createSwapChain(app);
    createFramebuffers(app);
}

int main() {
    VulkanApp app{};
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    app.window = glfwCreateWindow(WIDTH, HEIGHT, "Nightmare Graphics Stress Test", nullptr, nullptr);
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetFramebufferSizeCallback(app.window, framebufferResizeCallback);

    uint32_t count; const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    VkInstanceCreateInfo instInfo{}; instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.apiVersion = VK_API_VERSION_1_1; instInfo.pApplicationInfo = &appInfo; instInfo.enabledExtensionCount=count; instInfo.ppEnabledExtensionNames = extensions;
    if (vkCreateInstance(&instInfo, nullptr, &app.instance) != VK_SUCCESS) throw std::runtime_error("failed to create instance");
    if (glfwCreateWindowSurface(app.instance, app.window, nullptr, &app.surface) != VK_SUCCESS) throw std::runtime_error("failed to create window surface");
    vkEnumeratePhysicalDevices(app.instance, &count, nullptr); std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(app.instance, &count, devices.data()); app.physicalDevice = devices[0];
    vkGetPhysicalDeviceQueueFamilyProperties(app.physicalDevice, &count, nullptr); std::vector<VkQueueFamilyProperties> queueFamilies(count); vkGetPhysicalDeviceQueueFamilyProperties(app.physicalDevice, &count, queueFamilies.data());
    app.graphicsFamily = -1; for(uint32_t i=0; i<queueFamilies.size(); ++i) if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { app.graphicsFamily = i; break; }
    float qp=1.0f; VkDeviceQueueCreateInfo qcInfo{}; qcInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcInfo.queueFamilyIndex = app.graphicsFamily; qcInfo.queueCount=1; qcInfo.pQueuePriorities=&qp;
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; VkDeviceCreateInfo dInfo{}; dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dInfo.queueCreateInfoCount=1; dInfo.pQueueCreateInfos = &qcInfo; dInfo.enabledExtensionCount=1; dInfo.ppEnabledExtensionNames=deviceExtensions;
    if (vkCreateDevice(app.physicalDevice, &dInfo, nullptr, &app.device) != VK_SUCCESS) throw std::runtime_error("failed to create logical device");
    vkGetDeviceQueue(app.device, app.graphicsFamily, 0, &app.queue);

    VkAttachmentDescription colorAttachment{}; colorAttachment.format=VK_FORMAT_B8G8R8A8_SRGB; colorAttachment.samples=VK_SAMPLE_COUNT_1_BIT; colorAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; colorAttachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE; colorAttachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; colorAttachment.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorAttachmentRef{}; colorAttachmentRef.attachment=0; colorAttachmentRef.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount=1; subpass.pColorAttachments=&colorAttachmentRef;
    VkRenderPassCreateInfo rpInfo{}; rpInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; rpInfo.attachmentCount=1; rpInfo.pAttachments=&colorAttachment; rpInfo.subpassCount=1; rpInfo.pSubpasses=&subpass;
    if (vkCreateRenderPass(app.device, &rpInfo, nullptr, &app.renderPass) != VK_SUCCESS) throw std::runtime_error("failed to create render pass");

    createSwapChain(app);
    createFramebuffers(app);

    createImage(app.device, app.physicalDevice, NOISE_DIM, NOISE_DIM, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, app.noiseImage, app.noiseImageMemory);
    app.noiseImageView = createImageView(app.device, app.noiseImage, VK_FORMAT_R8G8B8A8_UNORM);
    VkSamplerCreateInfo sInfo{}; sInfo.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; sInfo.magFilter=VK_FILTER_LINEAR; sInfo.minFilter=VK_FILTER_LINEAR; sInfo.addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT; sInfo.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT; sInfo.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if (vkCreateSampler(app.device, &sInfo, nullptr, &app.sampler) != VK_SUCCESS) throw std::runtime_error("failed to create sampler");

    VkDescriptorPoolSize poolSizes[] = { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} };
    VkDescriptorPoolCreateInfo descPoolInfo{}; descPoolInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; descPoolInfo.poolSizeCount=2; descPoolInfo.pPoolSizes=poolSizes; descPoolInfo.maxSets=2;
    if (vkCreateDescriptorPool(app.device, &descPoolInfo, nullptr, &app.descriptorPool) != VK_SUCCESS) throw std::runtime_error("failed to create descriptor pool");

    VkCommandPoolCreateInfo poolInfo{}; poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; poolInfo.queueFamilyIndex=app.graphicsFamily; if (vkCreateCommandPool(app.device, &poolInfo, nullptr, &app.commandPool) != VK_SUCCESS) throw std::runtime_error("failed to create command pool");
    app.commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cbAllocInfo{}; cbAllocInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbAllocInfo.commandPool=app.commandPool; cbAllocInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT; if (vkAllocateCommandBuffers(app.device, &cbAllocInfo, app.commandBuffers.data()) != VK_SUCCESS) throw std::runtime_error("failed to allocate command buffers");
    app.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); app.renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); app.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo semInfo{}; semInfo.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; VkFenceCreateInfo fenceInfo{}; fenceInfo.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fenceInfo.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) { vkCreateSemaphore(app.device, &semInfo, nullptr, &app.imageAvailableSemaphores[i]); vkCreateSemaphore(app.device, &semInfo, nullptr, &app.renderFinishedSemaphores[i]); vkCreateFence(app.device, &fenceInfo, nullptr, &app.inFlightFences[i]); }

    { // One-time compute pass scope
        VkShaderModule noiseGenModule; createShaderModule(app.device, noise_generator_spv, sizeof(noise_generator_spv), &noiseGenModule);
        VkDescriptorSetLayoutBinding cBinding{}; cBinding.binding=0; cBinding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; cBinding.descriptorCount=1; cBinding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo cslInfo{}; cslInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; cslInfo.bindingCount=1; cslInfo.pBindings=&cBinding;
        VkDescriptorSetLayout computeSetLayout; vkCreateDescriptorSetLayout(app.device, &cslInfo, nullptr, &computeSetLayout);
        VkPipelineLayout computePipelineLayout; VkPipelineLayoutCreateInfo cplInfo{}; cplInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; cplInfo.setLayoutCount=1; cplInfo.pSetLayouts=&computeSetLayout; vkCreatePipelineLayout(app.device, &cplInfo, nullptr, &computePipelineLayout);
        VkPipeline computePipeline; VkComputePipelineCreateInfo computePipelineInfo{}; computePipelineInfo.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; computePipelineInfo.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; computePipelineInfo.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; computePipelineInfo.stage.module=noiseGenModule; computePipelineInfo.stage.pName="main"; computePipelineInfo.layout=computePipelineLayout; vkCreateComputePipelines(app.device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &computePipeline);
        VkDescriptorSet computeDescriptorSet; VkDescriptorSetAllocateInfo cdsAllocInfo{}; cdsAllocInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; cdsAllocInfo.descriptorPool=app.descriptorPool; cdsAllocInfo.descriptorSetCount=1; cdsAllocInfo.pSetLayouts=&computeSetLayout; vkAllocateDescriptorSets(app.device, &cdsAllocInfo, &computeDescriptorSet);
        VkDescriptorImageInfo cImageInfo{}; cImageInfo.imageView=app.noiseImageView; cImageInfo.imageLayout=VK_IMAGE_LAYOUT_GENERAL; VkWriteDescriptorSet cWrite{}; cWrite.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; cWrite.dstSet=computeDescriptorSet; cWrite.dstBinding=0; cWrite.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; cWrite.descriptorCount=1; cWrite.pImageInfo=&cImageInfo; vkUpdateDescriptorSets(app.device, 1, &cWrite, 0, nullptr);
        std::cout << "Generating noise texture on GPU..." << std::endl;
        transitionImageLayout(app.device, app.queue, app.commandPool, app.noiseImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        VkCommandBuffer compCb = beginSingleTimeCommands(app.device, app.commandPool);
        vkCmdBindPipeline(compCb, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline); vkCmdBindDescriptorSets(compCb, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr); vkCmdDispatch(compCb, NOISE_DIM/16, NOISE_DIM/16, 1);
        endSingleTimeCommands(app.device, app.queue, app.commandPool, compCb);
        transitionImageLayout(app.device, app.queue, app.commandPool, app.noiseImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        std::cout << "Noise texture generated." << std::endl;
        vkDestroyPipeline(app.device, computePipeline, nullptr); vkDestroyPipelineLayout(app.device, computePipelineLayout, nullptr); vkDestroyDescriptorSetLayout(app.device, computeSetLayout, nullptr); vkDestroyShaderModule(app.device, noiseGenModule, nullptr);
    }

    VkShaderModule vertModule, fragModule; createShaderModule(app.device, shader_vert_spv, sizeof(shader_vert_spv), &vertModule); createShaderModule(app.device, shader_frag_spv, sizeof(shader_frag_spv), &fragModule);
    VkPipelineShaderStageCreateInfo stages[2]{}; stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertModule; stages[0].pName="main"; stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=fragModule; stages[1].pName="main";
    VkDescriptorSetLayoutBinding gBinding{}; gBinding.binding=0; gBinding.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; gBinding.descriptorCount=1; gBinding.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo gslInfo{}; gslInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; gslInfo.bindingCount=1; gslInfo.pBindings=&gBinding;
    vkCreateDescriptorSetLayout(app.device, &gslInfo, nullptr, &app.graphicsSetLayout);
    VkPushConstantRange pushConstantRange{}; pushConstantRange.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT; pushConstantRange.size=sizeof(float);
    VkPipelineLayoutCreateInfo gplInfo{}; gplInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; gplInfo.setLayoutCount=1; gplInfo.pSetLayouts=&app.graphicsSetLayout; gplInfo.pushConstantRangeCount=1; gplInfo.pPushConstantRanges=&pushConstantRange;
    vkCreatePipelineLayout(app.device, &gplInfo, nullptr, &app.graphicsPipelineLayout);
    VkGraphicsPipelineCreateInfo pInfo{}; pInfo.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; pInfo.stageCount=2; pInfo.pStages=stages;
    VkPipelineVertexInputStateCreateInfo vi{}; vi.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO; pInfo.pVertexInputState=&vi;
    VkPipelineInputAssemblyStateCreateInfo ia{}; ia.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; pInfo.pInputAssemblyState=&ia;
    VkPipelineViewportStateCreateInfo vp{}; vp.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vp.viewportCount=1; vp.scissorCount=1; pInfo.pViewportState=&vp;
    VkPipelineRasterizationStateCreateInfo rs{}; rs.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; rs.lineWidth=1.0f; pInfo.pRasterizationState=&rs;
    VkPipelineMultisampleStateCreateInfo ms{}; ms.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT; pInfo.pMultisampleState=&ms;
    VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = 0xF; cba.blendEnable=VK_TRUE; cba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA; cba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE; cba.colorBlendOp=VK_BLEND_OP_ADD; cba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE; cba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO; cba.alphaBlendOp=VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo cb{}; cb.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cb.attachmentCount=1; cb.pAttachments=&cba; pInfo.pColorBlendState=&cb;
    pInfo.layout=app.graphicsPipelineLayout; pInfo.renderPass=app.renderPass;
    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{}; dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()); dynamicState.pDynamicStates = dynamicStates.data(); pInfo.pDynamicState = &dynamicState;
    vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &pInfo, nullptr, &app.graphicsPipeline);
    VkDescriptorSetAllocateInfo gdsAllocInfo{}; gdsAllocInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; gdsAllocInfo.descriptorPool=app.descriptorPool; gdsAllocInfo.descriptorSetCount=1; gdsAllocInfo.pSetLayouts=&app.graphicsSetLayout;
    vkAllocateDescriptorSets(app.device, &gdsAllocInfo, &app.graphicsDescriptorSet);
    VkDescriptorImageInfo gImageInfo{}; gImageInfo.imageView=app.noiseImageView; gImageInfo.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; gImageInfo.sampler=app.sampler;
    VkWriteDescriptorSet gWrite{}; gWrite.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; gWrite.dstSet=app.graphicsDescriptorSet; gWrite.dstBinding=0; gWrite.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; gWrite.descriptorCount=1; gWrite.pImageInfo=&gImageInfo;
    vkUpdateDescriptorSets(app.device, 1, &gWrite, 0, nullptr);
    vkDestroyShaderModule(app.device, vertModule, nullptr); vkDestroyShaderModule(app.device, fragModule, nullptr);


    std::cout << "Starting Resizable Nightmare Graphics Stress Test..." << std::endl;
    int currentFrame = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    // *** NEW: Variables for FPS calculation ***
    double lastFPSTime = glfwGetTime();
    int frameCount = 0;

    while(!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        vkWaitForFences(app.device, 1, &app.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(app.device, app.swapchain, UINT64_MAX, app.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain(app);
            continue;
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(app.device, 1, &app.inFlightFences[currentFrame]);
        vkResetCommandBuffer(app.commandBuffers[currentFrame], 0);
        VkCommandBuffer renderCb = app.commandBuffers[currentFrame];
        VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(renderCb, &beginInfo);
        VkRenderPassBeginInfo rpbInfo{}; rpbInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; rpbInfo.renderPass=app.renderPass; rpbInfo.framebuffer=app.framebuffers[imageIndex]; rpbInfo.renderArea.extent=app.swapchainExtent;
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.05f, 1.0f}}};
        rpbInfo.clearValueCount=1; rpbInfo.pClearValues=&clearColor;
        vkCmdBeginRenderPass(renderCb, &rpbInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(renderCb, VK_PIPELINE_BIND_POINT_GRAPHICS, app.graphicsPipeline);
        VkViewport viewport{}; viewport.width = (float)app.swapchainExtent.width; viewport.height = (float)app.swapchainExtent.height; viewport.maxDepth = 1.0f; vkCmdSetViewport(renderCb, 0, 1, &viewport);
        VkRect2D scissor{}; scissor.offset = {0,0}; scissor.extent = app.swapchainExtent; vkCmdSetScissor(renderCb, 0, 1, &scissor);
        vkCmdBindDescriptorSets(renderCb, VK_PIPELINE_BIND_POINT_GRAPHICS, app.graphicsPipelineLayout, 0, 1, &app.graphicsDescriptorSet, 0, nullptr);
        float time = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - startTime).count();
        vkCmdPushConstants(renderCb, app.graphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &time);
        vkCmdDraw(renderCb, 3, OVERDRAW_LAYERS, 0, 0);
        vkCmdEndRenderPass(renderCb);
        if (vkEndCommandBuffer(renderCb) != VK_SUCCESS) throw std::runtime_error("failed to record command buffer");

        VkSubmitInfo submitInfo{}; submitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {app.imageAvailableSemaphores[currentFrame]}; VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1; submitInfo.pWaitSemaphores = waitSemaphores; submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount=1; submitInfo.pCommandBuffers = &renderCb;
        VkSemaphore signalSemaphores[] = {app.renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1; submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(app.queue, 1, &submitInfo, app.inFlightFences[currentFrame]) != VK_SUCCESS) throw std::runtime_error("failed to submit draw command buffer");

        VkPresentInfoKHR presentInfo{}; presentInfo.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount=1; presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = {app.swapchain}; presentInfo.swapchainCount=1; presentInfo.pSwapchains = swapchains; presentInfo.pImageIndices = &imageIndex;
        result = vkQueuePresentKHR(app.queue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain(app);
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        // *** NEW: FPS calculation logic ***
        double currentTime = glfwGetTime();
        frameCount++;
        if (currentTime - lastFPSTime >= 1.0) {
            std::string title = "Nightmare Stress Test | " + std::to_string(frameCount) + " FPS";
            glfwSetWindowTitle(app.window, title.c_str());
            frameCount = 0;
            lastFPSTime = currentTime;
        }
    }

    vkDeviceWaitIdle(app.device);
    cleanupSwapChain(app);
    // ... Full cleanup of all other Vulkan objects ...
    glfwDestroyWindow(app.window);
    glfwTerminate();

    return 0;
}
