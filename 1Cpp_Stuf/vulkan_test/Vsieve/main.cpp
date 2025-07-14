#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <vulkan/vulkan.h>

// --- Configuration ---
const uint32_t BLOCK_SIZE = 1024 * 1024; // Test ~1 million numbers at a time.
const uint32_t WORKGROUP_SIZE = 256;

// Function to read a SPIR-V file
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

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

// Main application
int main() {
    // --- 1. Vulkan Setup (Instance, Device, Queue) ---
    VkInstance instance;
    VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.pApplicationName="Sieve"; appInfo.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo instInfo{}; instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; instInfo.pApplicationInfo = &appInfo;
    if (vkCreateInstance(&instInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Failed to create instance");

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No GPUs with Vulkan support found!");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    VkPhysicalDevice physicalDevice = devices[0];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    uint32_t queueFamilyIndex = -1;
    for(uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queueFamilyIndex = i;
            break;
        }
    }
    if (queueFamilyIndex == uint32_t(-1)) throw std::runtime_error("Failed to find a compute queue family!");

    VkDevice device;
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{}; queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; queueCreateInfo.queueFamilyIndex = queueFamilyIndex; queueCreateInfo.queueCount = 1; queueCreateInfo.pQueuePriorities = &queuePriority;
    VkDeviceCreateInfo devInfo{}; devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; devInfo.queueCreateInfoCount = 1; devInfo.pQueueCreateInfos = &queueCreateInfo;
    if (vkCreateDevice(physicalDevice, &devInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("Failed to create logical device");

    VkQueue computeQueue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &computeQueue);

    // --- 2. Create Result Buffer ---
    VkBuffer resultBuffer;
    VkDeviceMemory resultMemory;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(uint32_t) * BLOCK_SIZE;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufferInfo, nullptr, &resultBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, resultBuffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &resultMemory);
    vkBindBufferMemory(device, resultBuffer, resultMemory, 0);

    // --- 3. Create Compute Pipeline ---
    auto shaderCode = readFile("sieve.spv");
    VkShaderModule computeShaderModule;
    VkShaderModuleCreateInfo smInfo{}; smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smInfo.codeSize = shaderCode.size(); smInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    vkCreateShaderModule(device, &smInfo, nullptr, &computeShaderModule);

    VkDescriptorSetLayoutBinding binding{}; binding.binding = 0; binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binding.descriptorCount = 1; binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{}; layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; layoutInfo.bindingCount = 1; layoutInfo.pBindings = &binding;
    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint64_t);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{}; pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pipelineLayoutInfo.setLayoutCount = 1; pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; pipelineLayoutInfo.pushConstantRangeCount = 1; pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkPipeline computePipeline;
    VkComputePipelineCreateInfo pipelineInfo{}; pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pipelineInfo.stage.module = computeShaderModule; pipelineInfo.stage.pName = "main"; pipelineInfo.layout = pipelineLayout;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);

    // --- 4. Descriptor Set & Command Pool/Buffer ---
    VkDescriptorPool descriptorPool;
    VkDescriptorPoolSize poolSize{}; poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{}; poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize; poolInfo.maxSets = 1;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo dsAllocInfo{}; dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsAllocInfo.descriptorPool = descriptorPool; dsAllocInfo.descriptorSetCount = 1; dsAllocInfo.pSetLayouts = &descriptorSetLayout;
    vkAllocateDescriptorSets(device, &dsAllocInfo, &descriptorSet);

    VkDescriptorBufferInfo bufferDescInfo{}; bufferDescInfo.buffer = resultBuffer; bufferDescInfo.offset = 0; bufferDescInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet descriptorWrite{}; descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; descriptorWrite.dstSet = descriptorSet; descriptorWrite.dstBinding = 0; descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; descriptorWrite.descriptorCount = 1; descriptorWrite.pBufferInfo = &bufferDescInfo;
    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    VkCommandPool commandPool;
    VkCommandPoolCreateInfo cpInfo{}; cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpInfo.queueFamilyIndex = queueFamilyIndex;
    vkCreateCommandPool(device, &cpInfo, nullptr, &commandPool);

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo cbAllocInfo{}; cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbAllocInfo.commandPool = commandPool; cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &cbAllocInfo, &commandBuffer);

    // --- 5. Main Sieve Loop ---
    uint64_t current_offset = 0;
    std::cout << "Starting prime sieve. Press Ctrl+C to stop." << std::endl;

    while (true) {
        auto t1 = std::chrono::high_resolution_clock::now();

        VkCommandBufferBeginInfo beginInfo{}; beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        //
        // --- THIS IS THE LINE THAT MUST BE CORRECT ---
        //
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint64_t), &current_offset);

        vkCmdDispatch(commandBuffer, BLOCK_SIZE / WORKGROUP_SIZE, 1, 1);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(computeQueue);

        vkResetCommandBuffer(commandBuffer, 0);

        auto t2 = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(t2 - t1).count();

        void* data;
        vkMapMemory(device, resultMemory, 0, sizeof(uint32_t) * BLOCK_SIZE, 0, &data);
        uint32_t* results = static_cast<uint32_t*>(data);

        std::cout << "\n--- Block [" << current_offset << " - " << current_offset + BLOCK_SIZE - 1 << "] (" << BLOCK_SIZE/1e6 << "M nums) processed in " << duration << "s ---\n";
        for(uint32_t i=0; i < BLOCK_SIZE; ++i) {
            if (results[i] == 1) {
                std::cout << "Found prime: " << current_offset + i << "\n";
            }
        }
        vkUnmapMemory(device, resultMemory);

        current_offset += BLOCK_SIZE;
        if (current_offset > (UINT64_MAX - BLOCK_SIZE)) {
            std::cout << "Reached the limit of 64-bit integers. Stopping." << std::endl;
            break;
        }
    }

    // --- Cleanup ---
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyShaderModule(device, computeShaderModule, nullptr);
    vkDestroyBuffer(device, resultBuffer, nullptr);
    vkFreeMemory(device, resultMemory, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
