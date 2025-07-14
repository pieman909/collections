#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <vulkan/vulkan.h>

// --- Configuration ---
const uint32_t BIGNUM_WORDS_COUNT = 103810244;
const uint32_t MILLER_RABIN_ITERATIONS = 64;
const size_t BIGNUM_SIZE_BYTES = BIGNUM_WORDS_COUNT * sizeof(uint32_t);

// Function to read a SPIR-V file
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

// Main application
int main() {
    // --- 1. Initialize Vulkan Instance ---
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Primality Test";
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    // --- 2. Select Physical Device and Queue Family ---
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t computeQueueFamilyIndex = -1;

    for (const auto& device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                physicalDevice = device;
                computeQueueFamilyIndex = i;
                break;
            }
        }
        if (physicalDevice != VK_NULL_HANDLE) break;
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU with a compute queue!");
    }

    // --- 3. Create Logical Device and Queue ---
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    VkQueue computeQueue;
    vkGetDeviceQueue(device, computeQueueFamilyIndex, 0, &computeQueue);
    
    // --- 4. Create Buffers ---
    // Helper function for memory allocation
    auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    };

    // Create a single buffer
    auto createBuffer = [&](VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        
        // THIS ALLOCATION WILL LIKELY FAIL FOR THE HUGE NUMBER BUFFER
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory! The number is too large.");
        }
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    };

    VkBuffer numberBuffer, basesBuffer, resultBuffer;
    VkDeviceMemory numberMemory, basesMemory, resultMemory;

    // The impossible buffer
    std::cout << "Attempting to allocate " << BIGNUM_SIZE_BYTES / (1024*1024) << " MB for the number..." << std::endl;
    createBuffer(BIGNUM_SIZE_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, numberBuffer, numberMemory);
    
    // The bases buffer
    createBuffer(sizeof(uint32_t) * MILLER_RABIN_ITERATIONS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, basesBuffer, basesMemory);

    // The result buffer
    createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, resultBuffer, resultMemory);

    // --- 5. Prepare Data ---
    // The first 64 prime numbers as bases
    uint32_t primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311};
    void* data;
    vkMapMemory(device, basesMemory, 0, sizeof(primes), 0, &data);
    memcpy(data, primes, sizeof(primes));
    vkUnmapMemory(device, basesMemory);
    
    // NOTE: We don't write the huge number to the device buffer because it would
    // require a massive staging buffer and complicate the example. In a real
    // scenario, you would generate it and copy it to numberBuffer.

    // --- 6. Create Compute Pipeline ---
    auto shaderCode = readFile("primality.spv");
    VkShaderModuleCreateInfo shaderModuleInfo{};
    shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleInfo.codeSize = shaderCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    VkShaderModule computeShaderModule;
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &computeShaderModule);

    VkDescriptorSetLayoutBinding numberBinding{}, basesBinding{}, resultBinding{};
    numberBinding.binding = 0; numberBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; numberBinding.descriptorCount = 1; numberBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    basesBinding.binding = 1; basesBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; basesBinding.descriptorCount = 1; basesBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    resultBinding.binding = 2; resultBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; resultBinding.descriptorCount = 1; resultBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    std::vector<VkDescriptorSetLayoutBinding> bindings = {numberBinding, basesBinding, resultBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;
    VkPipeline computePipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);

    // --- 7. Create Descriptor Set ---
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 3;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = descriptorPool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(device, &allocSetInfo, &descriptorSet);

    VkDescriptorBufferInfo numberBufferInfo{}, basesBufferInfo{}, resultBufferInfo{};
    numberBufferInfo.buffer = numberBuffer; numberBufferInfo.offset = 0; numberBufferInfo.range = VK_WHOLE_SIZE;
    basesBufferInfo.buffer = basesBuffer; basesBufferInfo.offset = 0; basesBufferInfo.range = VK_WHOLE_SIZE;
    resultBufferInfo.buffer = resultBuffer; resultBufferInfo.offset = 0; resultBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet numberWrite{}, basesWrite{}, resultWrite{};
    numberWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; numberWrite.dstSet = descriptorSet; numberWrite.dstBinding = 0; numberWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; numberWrite.descriptorCount = 1; numberWrite.pBufferInfo = &numberBufferInfo;
    basesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; basesWrite.dstSet = descriptorSet; basesWrite.dstBinding = 1; basesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; basesWrite.descriptorCount = 1; basesWrite.pBufferInfo = &basesBufferInfo;
    resultWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; resultWrite.dstSet = descriptorSet; resultWrite.dstBinding = 2; resultWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; resultWrite.descriptorCount = 1; resultWrite.pBufferInfo = &resultBufferInfo;
    std::vector<VkWriteDescriptorSet> descriptorWrites = {numberWrite, basesWrite, resultWrite};
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    // --- 8. Record and Submit Command Buffer ---
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = computeQueueFamilyIndex;
    VkCommandPool commandPool;
    vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(commandBuffer, 1, 1, 1);
    vkEndCommandBuffer(commandBuffer);

    std::cout << "Submitting compute shader to the GPU..." << std::endl;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);

    std::cout << "Waiting for computation to finish..." << std::endl;
    std::cout << "!!! EXECUTION WILL HANG HERE INDEFINITELY. THIS IS EXPECTED. !!!" << std::endl;
    std::cout << "!!! Your GPU is now at 100% load performing the impossible calculation. !!!" << std::endl;
    std::cout << "!!! You will need to manually terminate the program (Ctrl+C). !!!" << std::endl;

    // THE POINT OF IMPOSSIBILITY
    vkQueueWaitIdle(computeQueue);

    // --- 9. Read Result (This code will never be reached) ---
    std::cout << "Computation finished!" << std::endl;
    vkMapMemory(device, resultMemory, 0, sizeof(uint32_t), 0, &data);
    uint32_t finalResult = *static_cast<uint32_t*>(data);
    vkUnmapMemory(device, resultMemory);

    if (finalResult == 1) {
        std::cout << "Result: The number is a prime candidate." << std::endl;
    } else {
        std::cout << "Result: The number is composite." << std::endl;
    }

    // --- 10. Cleanup ---
    vkDestroyBuffer(device, numberBuffer, nullptr);
    vkFreeMemory(device, numberMemory, nullptr);
    vkDestroyBuffer(device, basesBuffer, nullptr);
    vkFreeMemory(device, basesMemory, nullptr);
    vkDestroyBuffer(device, resultBuffer, nullptr);
    vkFreeMemory(device, resultMemory, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyShaderModule(device, computeShaderModule, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}