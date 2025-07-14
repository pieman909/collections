#include <iostream>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "shader.h" // Will be generated from hybrid_stress.comp

// --- Configuration ---
const int FRAMES_IN_FLIGHT = 2;
const uint32_t WORKGROUP_COUNT = 4096;
const uint32_t WORKGROUP_SIZE = 256;
const uint32_t BUFFER_ELEMENTS = 64 * 1024 * 1024; // 64 Million uints = 256 MB

// Helper to find a suitable memory type
uint32_t findMemoryType(VkPhysicalDevice pDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

// Helper to create a device-local buffer
void createDeviceBuffer(VkDevice device, VkPhysicalDevice pDevice, VkDeviceSize size, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(pDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    vkBindBufferMemory(device, buffer, memory, 0);
}


int main() {
    // --- 1. Vulkan Setup (Standard, Abridged) ---
    VkInstance instance; VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.pApplicationName="GPU Bandwidth Test"; appInfo.apiVersion = VK_API_VERSION_1_2; VkInstanceCreateInfo instInfo{}; instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; instInfo.pApplicationInfo = &appInfo; vkCreateInstance(&instInfo, nullptr, &instance); uint32_t deviceCount = 0; vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); std::vector<VkPhysicalDevice> devices(deviceCount); vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()); VkPhysicalDevice physicalDevice = devices[0]; uint32_t queueFamilyIndex=0; vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceCount, nullptr); std::vector<VkQueueFamilyProperties> queueFamilies(deviceCount); vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceCount, queueFamilies.data()); for(uint32_t i=0; i<deviceCount; ++i) if(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { queueFamilyIndex = i; break; } VkDevice device; float qp=1.0f; VkDeviceQueueCreateInfo qcInfo{}; qcInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qcInfo.queueFamilyIndex = queueFamilyIndex; qcInfo.queueCount = 1; qcInfo.pQueuePriorities = &qp; VkDeviceCreateInfo dInfo{}; dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dInfo.queueCreateInfoCount = 1; dInfo.pQueueCreateInfos = &qcInfo; vkCreateDevice(physicalDevice, &dInfo, nullptr, &device); VkQueue computeQueue; vkGetDeviceQueue(device, queueFamilyIndex, 0, &computeQueue);

    // --- 2. Create Two Large Device-Local Buffers ---
    VkBuffer inputBuffer, outputBuffer;
    VkDeviceMemory inputMemory, outputMemory;
    VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * BUFFER_ELEMENTS;

    std::cout << "Allocating " << (bufferSizeBytes * 2) / (1024*1024) << " MiB of VRAM for stress test..." << std::endl;
    createDeviceBuffer(device, physicalDevice, bufferSizeBytes, inputBuffer, inputMemory);
    createDeviceBuffer(device, physicalDevice, bufferSizeBytes, outputBuffer, outputMemory);

    // --- 3. Create Pipeline for Two Buffers ---
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo smInfo{}; smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smInfo.codeSize = sizeof(hybrid_stress_spv); smInfo.pCode = reinterpret_cast<const uint32_t*>(hybrid_stress_spv);
    vkCreateShaderModule(device, &smInfo, nullptr, &shaderModule);

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0; bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; bindings[0].descriptorCount = 1; bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1; bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; bindings[1].descriptorCount = 1; bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{}; layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; layoutInfo.bindingCount = 2; layoutInfo.pBindings = bindings;
    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo plInfo{}; plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; plInfo.setLayoutCount = 1; plInfo.pSetLayouts = &descriptorSetLayout; plInfo.pushConstantRangeCount = 1; plInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &plInfo, nullptr, &pipelineLayout);

    VkComputePipelineCreateInfo pInfo{}; pInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; pInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; pInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pInfo.stage.module = shaderModule; pInfo.stage.pName = "main"; pInfo.layout = pipelineLayout;
    VkPipeline pipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pInfo, nullptr, &pipeline);

    // --- 4. Descriptor Set & Pipelining Resources ---
    VkDescriptorPool pool;
    VkDescriptorPoolSize poolSize{}; poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; poolSize.descriptorCount = 2;
    VkDescriptorPoolCreateInfo poolInfo{}; poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize; poolInfo.maxSets = 1;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);

    VkDescriptorSet dSet;
    VkDescriptorSetAllocateInfo dsAllocInfo{}; dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsAllocInfo.descriptorPool = pool; dsAllocInfo.descriptorSetCount = 1; dsAllocInfo.pSetLayouts = &descriptorSetLayout;
    vkAllocateDescriptorSets(device, &dsAllocInfo, &dSet);

    VkDescriptorBufferInfo inputBufInfo{}, outputBufInfo{};
    inputBufInfo.buffer = inputBuffer; inputBufInfo.range = VK_WHOLE_SIZE;
    outputBufInfo.buffer = outputBuffer; outputBufInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[0].dstSet = dSet; writes[0].dstBinding = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].descriptorCount = 1; writes[0].pBufferInfo = &inputBufInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[1].dstSet = dSet; writes[1].dstBinding = 1; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[1].descriptorCount = 1; writes[1].pBufferInfo = &outputBufInfo;
    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    VkCommandPool cmdPool;
    VkCommandPoolCreateInfo cpInfo{}; cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpInfo.queueFamilyIndex = queueFamilyIndex;
    vkCreateCommandPool(device, &cpInfo, nullptr, &cmdPool);

    std::vector<VkCommandBuffer> cmdBuffers(FRAMES_IN_FLIGHT);
    std::vector<VkFence> fences(FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cbAllocInfo{}; cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbAllocInfo.commandPool = cmdPool; cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbAllocInfo.commandBufferCount = FRAMES_IN_FLIGHT;
    vkAllocateCommandBuffers(device, &cbAllocInfo, cmdBuffers.data());
    VkFenceCreateInfo fenceInfo{}; fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i=0; i<FRAMES_IN_FLIGHT; ++i) vkCreateFence(device, &fenceInfo, nullptr, &fences[i]);

    // --- 5. The Unthrottled Memory Stress Loop ---
    std::cout << "*******************************************\n" << "***  MEMORY BANDWIDTH STRESS TEST RUNNING ***\n" << "***    Monitor GPU Power (Watts).         ***\n" << "***        Press Ctrl+C to exit.        ***\n" << "*******************************************" << std::endl;

    int frame = 0;
    while (true) {
        vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &fences[frame]);
        VkCommandBuffer cb = cmdBuffers[frame];
        vkResetCommandBuffer(cb, 0);
        VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cb, &beginInfo);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &dSet, 0, nullptr);
        uint32_t buffer_elements_const = BUFFER_ELEMENTS;
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &buffer_elements_const);
        vkCmdDispatch(cb, WORKGROUP_COUNT, 1, 1);
        vkEndCommandBuffer(cb);
        VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &cb;
        vkQueueSubmit(computeQueue, 1, &submitInfo, fences[frame]);
        frame = (frame + 1) % FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);
    return 0;
}
