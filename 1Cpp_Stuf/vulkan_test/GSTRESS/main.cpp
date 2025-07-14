#include <iostream>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "shader.h" // Will be generated from stress.comp

// --- Configuration ---
const int FRAMES_IN_FLIGHT = 2;
const uint32_t WORKGROUP_COUNT = 4096;
const uint32_t WORKGROUP_SIZE = 256;

// Helper to find a suitable memory type
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

int main() {
    // --- 1. Vulkan Setup (Standard, Abridged) ---
    // ... This section is identical to the previous version and is correct ...
    VkInstance instance; VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.pApplicationName="GPU Stress Test"; appInfo.apiVersion = VK_API_VERSION_1_2; VkInstanceCreateInfo instInfo{}; instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; instInfo.pApplicationInfo = &appInfo; vkCreateInstance(&instInfo, nullptr, &instance); uint32_t deviceCount = 0; vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); if (deviceCount == 0) throw std::runtime_error("No GPUs with Vulkan support found!"); std::vector<VkPhysicalDevice> devices(deviceCount); vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()); VkPhysicalDevice physicalDevice = devices[0]; uint32_t queueFamilyIndex = 0; vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceCount, nullptr); std::vector<VkQueueFamilyProperties> queueFamilies(deviceCount); vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceCount, queueFamilies.data()); for(uint32_t i=0; i<deviceCount; ++i) if(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { queueFamilyIndex = i; break; } VkDevice device; float queuePriority = 1.0f; VkDeviceQueueCreateInfo queueCreateInfo{}; queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; queueCreateInfo.queueFamilyIndex = queueFamilyIndex; queueCreateInfo.queueCount = 1; queueCreateInfo.pQueuePriorities = &queuePriority; VkDeviceCreateInfo devInfo{}; devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; devInfo.queueCreateInfoCount = 1; devInfo.pQueueCreateInfos = &queueCreateInfo; vkCreateDevice(physicalDevice, &devInfo, nullptr, &device); VkQueue computeQueue; vkGetDeviceQueue(device, queueFamilyIndex, 0, &computeQueue);

    // --- 2. Create Vulkan Resources (Standard) ---
    // ... This section is also identical and correct ...
    VkBuffer resultBuffer; VkDeviceMemory resultMemory; const size_t bufferSize = sizeof(uint32_t) * WORKGROUP_COUNT * WORKGROUP_SIZE; VkBufferCreateInfo bufferInfo{}; bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bufferInfo.size = bufferSize; bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; vkCreateBuffer(device, &bufferInfo, nullptr, &resultBuffer); VkMemoryRequirements memRequirements; vkGetBufferMemoryRequirements(device, resultBuffer, &memRequirements); VkMemoryAllocateInfo allocInfo{}; allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; allocInfo.allocationSize = memRequirements.size; allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); vkAllocateMemory(device, &allocInfo, nullptr, &resultMemory); vkBindBufferMemory(device, resultBuffer, resultMemory, 0); VkShaderModule computeShaderModule; VkShaderModuleCreateInfo smInfo{}; smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smInfo.codeSize = sizeof(stress_spv); smInfo.pCode = reinterpret_cast<const uint32_t*>(stress_spv); vkCreateShaderModule(device, &smInfo, nullptr, &computeShaderModule); VkDescriptorSetLayoutBinding binding{}; binding.binding = 0; binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binding.descriptorCount = 1; binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; VkDescriptorSetLayoutCreateInfo layoutInfo{}; layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; layoutInfo.bindingCount = 1; layoutInfo.pBindings = &binding; VkDescriptorSetLayout descriptorSetLayout; vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout); VkPipelineLayoutCreateInfo pipelineLayoutInfo{}; pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pipelineLayoutInfo.setLayoutCount = 1; pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; VkPipelineLayout pipelineLayout; vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout); VkPipeline computePipeline; VkComputePipelineCreateInfo pipelineInfo{}; pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pipelineInfo.stage.module = computeShaderModule; pipelineInfo.stage.pName = "main"; pipelineInfo.layout = pipelineLayout; vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline); VkDescriptorPool descriptorPool; VkDescriptorPoolSize poolSize{}; poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; poolSize.descriptorCount = 1; VkDescriptorPoolCreateInfo poolInfo{}; poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize; poolInfo.maxSets = 1; vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool); VkDescriptorSet descriptorSet; VkDescriptorSetAllocateInfo dsAllocInfo{}; dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsAllocInfo.descriptorPool = descriptorPool; dsAllocInfo.descriptorSetCount = 1; dsAllocInfo.pSetLayouts = &descriptorSetLayout; vkAllocateDescriptorSets(device, &dsAllocInfo, &descriptorSet); VkDescriptorBufferInfo bufferDescInfo{}; bufferDescInfo.buffer = resultBuffer; bufferDescInfo.offset = 0; bufferDescInfo.range = VK_WHOLE_SIZE; VkWriteDescriptorSet descriptorWrite{}; descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; descriptorWrite.dstSet = descriptorSet; descriptorWrite.dstBinding = 0; descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; descriptorWrite.descriptorCount = 1; descriptorWrite.pBufferInfo = &bufferDescInfo; vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr); VkCommandPool commandPool; VkCommandPoolCreateInfo cpInfo{}; cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpInfo.queueFamilyIndex = queueFamilyIndex; vkCreateCommandPool(device, &cpInfo, nullptr, &commandPool); std::vector<VkCommandBuffer> commandBuffers(FRAMES_IN_FLIGHT); std::vector<VkFence> inFlightFences(FRAMES_IN_FLIGHT); VkCommandBufferAllocateInfo cbAllocInfo{}; cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbAllocInfo.commandPool = commandPool; cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbAllocInfo.commandBufferCount = (uint32_t)FRAMES_IN_FLIGHT; vkAllocateCommandBuffers(device, &cbAllocInfo, commandBuffers.data()); VkFenceCreateInfo fenceInfo{}; fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; for (int i = 0; i < FRAMES_IN_FLIGHT; i++) { vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]); }

    // --- 3. The Final, Unthrottled Stress Loop ---
    std::cout << "*******************************************" << std::endl;
    std::cout << "***     GPU STRESS TEST IS RUNNING      ***" << std::endl;
    std::cout << "***  This is the maximum throughput test.   ***"
    "\n***     Press Ctrl+C to exit.         ***" << std::endl;
    std::cout << "*******************************************" << std::endl;

    int currentFrame = 0;
    while (true) {
        // Wait for the frame's resources to be free
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        // Record the command buffer
        VkCommandBuffer cb = commandBuffers[currentFrame];
        vkResetCommandBuffer(cb, 0);
        VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cb, &beginInfo);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDispatch(cb, WORKGROUP_COUNT, 1, 1);
        vkEndCommandBuffer(cb);

        // Submit to the GPU. THERE IS NO SLEEP.
        VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &cb;
        vkQueueSubmit(computeQueue, 1, &submitInfo, inFlightFences[currentFrame]);

        // Move to the next frame
        currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
    }

    // --- 4. Cleanup ---
    vkDeviceWaitIdle(device);
    // ... all vkDestroy calls ...
    return 0;
}
