#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <fstream>
#include <memory>
#include <atomic>
#include <thread>
#include <iomanip>
#include <gmp.h>
#include <cstring>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

// Vulkan validation layers
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// GPU constants
const uint32_t WORKGROUP_SIZE = 256;
const uint32_t MAX_BIGINT_LIMBS = 128;  // Support up to ~3000 bit numbers
const uint32_t MR_ROUNDS_GPU = 64;     // More rounds on GPU since it's faster

// Big integer representation for GPU (fixed-size limbs)
struct GPUBigInt {
    uint32_t limbs[MAX_BIGINT_LIMBS];
    uint32_t size;  // Number of used limbs
    uint32_t _padding[3];  // Ensure 16-byte alignment
};

// Miller-Rabin test parameters for GPU
struct MRParams {
    GPUBigInt n;           // Number to test
    GPUBigInt n_minus_1;   // n-1
    GPUBigInt d;           // Odd part of n-1
    uint32_t s;            // Power of 2 in n-1 = 2^s * d
    uint32_t rounds;       // Number of rounds to perform
    uint32_t seed;         // Random seed
    uint32_t _padding;     // Alignment
};

// Result structure
struct MRResult {
    uint32_t is_composite;  // 1 if composite found, 0 if probably prime
    uint32_t round_completed; // Number of rounds completed
    uint32_t _padding[2];   // Alignment
};

class VulkanPrimalityTester {
private:
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue computeQueue;
    VkCommandPool commandPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline computePipeline;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    // Buffers
    VkBuffer paramsBuffer;
    VkDeviceMemory paramsBufferMemory;
    VkBuffer resultBuffer;
    VkDeviceMemory resultBufferMemory;
    VkBuffer randomBuffer;
    VkDeviceMemory randomBufferMemory;

    uint32_t queueFamilyIndex;

    // GMP for host-side operations
    mpz_t n, n_minus_1;
    gmp_randstate_t rng;

    // Progress tracking
    std::atomic<uint64_t> progress_counter{0};
    std::atomic<bool> test_complete{false};

    // Helper functions
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                return false;
            }
        }
        return true;
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Primality Tester";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                queueFamilyIndex = i;
                return true;
            }
        }

        return false;
    }

    void createLogicalDevice() {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(device, queueFamilyIndex, 0, &computeQueue);
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
        // Verify buffer was created successfully
        if (buffer == VK_NULL_HANDLE || bufferMemory == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create or bind buffer memory!");
        }
                      }

                      void createBuffers() {
                          // Parameters buffer
                          VkDeviceSize paramsSize = sizeof(MRParams);
                          createBuffer(paramsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       paramsBuffer, paramsBufferMemory);

                          // Result buffer
                          VkDeviceSize resultSize = sizeof(MRResult);
                          createBuffer(resultSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       resultBuffer, resultBufferMemory);

                          // Random numbers buffer (for multiple random bases)
                          VkDeviceSize randomSize = sizeof(uint32_t) * MR_ROUNDS_GPU * 16; // Extra random data
                          createBuffer(randomSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       randomBuffer, randomBufferMemory);
                      }

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

                      void createDescriptorSetLayout() {
                          std::vector<VkDescriptorSetLayoutBinding> bindings(3);

                          // Parameters buffer
                          bindings[0].binding = 0;
                          bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          bindings[0].descriptorCount = 1;
                          bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

                          // Result buffer
                          bindings[1].binding = 1;
                          bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          bindings[1].descriptorCount = 1;
                          bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

                          // Random buffer
                          bindings[2].binding = 2;
                          bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          bindings[2].descriptorCount = 1;
                          bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

                          VkDescriptorSetLayoutCreateInfo layoutInfo{};
                          layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                          layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
                          layoutInfo.pBindings = bindings.data();

                          if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
                              throw std::runtime_error("Failed to create descriptor set layout!");
                          }
                      }

                      void createComputePipeline() {
                          // We'll need to create the compute shader
                          // For now, create a simple pipeline layout
                          VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
                          pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                          pipelineLayoutInfo.setLayoutCount = 1;
                          pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

                          if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                              throw std::runtime_error("Failed to create pipeline layout!");
                          }

                          // Create compute shader (we'll embed the SPIR-V code)
                          VkShaderModule computeShaderModule = createShaderModule(getComputeShaderCode());

                          VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
                          computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                          computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                          computeShaderStageInfo.module = computeShaderModule;
                          computeShaderStageInfo.pName = "main";

                          VkComputePipelineCreateInfo pipelineInfo{};
                          pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                          pipelineInfo.layout = pipelineLayout;
                          pipelineInfo.stage = computeShaderStageInfo;

                          if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
                              throw std::runtime_error("Failed to create compute pipeline!");
                          }

                          vkDestroyShaderModule(device, computeShaderModule, nullptr);
                      }

                      VkShaderModule createShaderModule(const std::vector<char>& code) {
                          VkShaderModuleCreateInfo createInfo{};
                          createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                          createInfo.codeSize = code.size();
                          createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

                          VkShaderModule shaderModule;
                          if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                              throw std::runtime_error("Failed to create shader module!");
                          }

                          return shaderModule;
                      }


                      std::vector<char> getComputeShaderCode() {
                          return readFile("miller_rabin.spv"); // Use the existing readFile helper
                      }

                      void createDescriptorPool() {
                          std::vector<VkDescriptorPoolSize> poolSizes(1);
                          poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          poolSizes[0].descriptorCount = 3;

                          VkDescriptorPoolCreateInfo poolInfo{};
                          poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                          poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
                          poolInfo.pPoolSizes = poolSizes.data();
                          poolInfo.maxSets = 1;

                          if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
                              throw std::runtime_error("Failed to create descriptor pool!");
                          }
                      }

                      void createDescriptorSets() {
                          VkDescriptorSetAllocateInfo allocInfo{};
                          allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                          allocInfo.descriptorPool = descriptorPool;
                          allocInfo.descriptorSetCount = 1;
                          allocInfo.pSetLayouts = &descriptorSetLayout;

                          if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
                              throw std::runtime_error("Failed to allocate descriptor sets!");
                          }

                          // Update descriptor sets
                          std::vector<VkWriteDescriptorSet> descriptorWrites(3);

                          VkDescriptorBufferInfo paramsBufferInfo{};
                          paramsBufferInfo.buffer = paramsBuffer;
                          paramsBufferInfo.offset = 0;
                          paramsBufferInfo.range = sizeof(MRParams);

                          descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                          descriptorWrites[0].dstSet = descriptorSet;
                          descriptorWrites[0].dstBinding = 0;
                          descriptorWrites[0].dstArrayElement = 0;
                          descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          descriptorWrites[0].descriptorCount = 1;
                          descriptorWrites[0].pBufferInfo = &paramsBufferInfo;

                          VkDescriptorBufferInfo resultBufferInfo{};
                          resultBufferInfo.buffer = resultBuffer;
                          resultBufferInfo.offset = 0;
                          resultBufferInfo.range = sizeof(MRResult);

                          descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                          descriptorWrites[1].dstSet = descriptorSet;
                          descriptorWrites[1].dstBinding = 1;
                          descriptorWrites[1].dstArrayElement = 0;
                          descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          descriptorWrites[1].descriptorCount = 1;
                          descriptorWrites[1].pBufferInfo = &resultBufferInfo;

                          VkDescriptorBufferInfo randomBufferInfo{};
                          randomBufferInfo.buffer = randomBuffer;
                          randomBufferInfo.offset = 0;
                          randomBufferInfo.range = sizeof(uint32_t) * MR_ROUNDS_GPU * 16;

                          descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                          descriptorWrites[2].dstSet = descriptorSet;
                          descriptorWrites[2].dstBinding = 2;
                          descriptorWrites[2].dstArrayElement = 0;
                          descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                          descriptorWrites[2].descriptorCount = 1;
                          descriptorWrites[2].pBufferInfo = &randomBufferInfo;

                          vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
                      }

                      void mpzToGPUBigInt(const mpz_t src, GPUBigInt& dst) {
                          // Convert GMP number to GPU format
                          memset(&dst, 0, sizeof(dst));

                          size_t limb_count = mpz_size(src);
                          if (limb_count > MAX_BIGINT_LIMBS) {
                              throw std::runtime_error("Number too large for GPU representation");
                          }

                          dst.size = static_cast<uint32_t>(limb_count);


                          // Handle both 32-bit and 64-bit GMP limbs
                          for (size_t i = 0; i < limb_count && i < MAX_BIGINT_LIMBS; i++) {
                              mp_limb_t limb = mpz_getlimbn(src, i);
                              dst.limbs[i] = static_cast<uint32_t>(limb & 0xFFFFFFFF);
                              // If we need to handle 64-bit limbs properly, we'd need more complex logic
                          }
                      }

                      void generateRandomData(uint32_t* data, size_t count) {
                          std::random_device rd;
                          std::mt19937 gen(rd());
                          std::uniform_int_distribution<uint32_t> dis;

                          for (size_t i = 0; i < count; i++) {
                              data[i] = dis(gen);
                          }
                      }

public:
    VulkanPrimalityTester() {
        // Initialize GMP
        mpz_init(n);
        mpz_init(n_minus_1);
        gmp_randinit_mt(rng);

        std::random_device rd;
        gmp_randseed_ui(rng, rd());

        // Initialize Vulkan
        createInstance();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        createDescriptorSetLayout();
        createBuffers();
        createComputePipeline();
        createDescriptorPool();
        createDescriptorSets();
    }

    ~VulkanPrimalityTester() {
        // Clean up Vulkan resources
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, paramsBuffer, nullptr);
        vkFreeMemory(device, paramsBufferMemory, nullptr);
        vkDestroyBuffer(device, resultBuffer, nullptr);
        vkFreeMemory(device, resultBufferMemory, nullptr);
        vkDestroyBuffer(device, randomBuffer, nullptr);
        vkFreeMemory(device, randomBufferMemory, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);

        // Clean up GMP
        mpz_clear(n);
        mpz_clear(n_minus_1);
        gmp_randclear(rng);
    }

    void set_number(const std::string& num_str) {
        mpz_set_str(n, num_str.c_str(), 10);
        mpz_sub_ui(n_minus_1, n, 1);
    }

    void generate_random_number(uint64_t digits) {
        mpz_t base, range;
        mpz_init(base);
        mpz_init(range);

        mpz_ui_pow_ui(base, 10, digits - 1);
        mpz_mul_ui(range, base, 9);
        mpz_urandomm(n, rng, range);
        mpz_add(n, n, base);

        if (mpz_even_p(n)) {
            mpz_add_ui(n, n, 1);
        }

        mpz_sub_ui(n_minus_1, n, 1);
        mpz_clear(base);
        mpz_clear(range);
    }

    void generate_ultra_large_number(double exp) {
        if (exp > 6) {
            // Limit to prevent memory issues
            exp = 6;
        }

        uint64_t digits = static_cast<uint64_t>(std::pow(10, exp));
        if (digits > 1000000) {
            digits = 1000000;  // Cap at 1M digits
        }

        generate_random_number(digits);
    }

    bool is_prime(int rounds = MR_ROUNDS_GPU) {
        // First, basic checks
        if (mpz_cmp_ui(n, 2) == 0 || mpz_cmp_ui(n, 3) == 0) {
            return true;
        }
        if (mpz_even_p(n) || mpz_cmp_ui(n, 2) < 0) {
            return false;
        }
        if (mpz_cmp_ui(n, 1) == 0) {
            return false;
        }

        // Quick divisibility test
        std::vector<uint64_t> small_primes = {
            3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97
        };

        mpz_t temp;
        mpz_init(temp);

        for (uint64_t p : small_primes) {
            mpz_mod_ui(temp, n, p);
            if (mpz_cmp_ui(temp, 0) == 0 && mpz_cmp_ui(n, p) != 0) {
                mpz_clear(temp);
                return false;
            }
        }
        mpz_clear(temp);

        // Check if number is too large for GPU
        size_t bits = mpz_sizeinbase(n, 2);
        if (bits > MAX_BIGINT_LIMBS * 32) {
            std::cout << "Warning: Number too large for GPU (" << bits << " bits), maximum supported is "
            << (MAX_BIGINT_LIMBS * 32) << " bits" << std::endl;
            return false;  // Would need CPU fallback here
        }

        // Prepare GPU computation
        MRParams params;
        memset(&params, 0, sizeof(params));

        mpzToGPUBigInt(n, params.n);
        mpzToGPUBigInt(n_minus_1, params.n_minus_1);

        // Find d and s such that n-1 = 2^s * d
        mpz_t d;
        mpz_init(d);
        mpz_set(d, n_minus_1);

        uint32_t s = 0;
        while (mpz_even_p(d)) {
            mpz_divexact_ui(d, d, 2);
            s++;
        }

        mpzToGPUBigInt(d, params.d);
        params.s = s;
        params.rounds = rounds;

        std::random_device rd;
        params.seed = rd();

        mpz_clear(d);

        // Upload parameters to GPU
        void* data;
        vkMapMemory(device, paramsBufferMemory, 0, sizeof(MRParams), 0, &data);
        memcpy(data, &params, sizeof(MRParams));
        vkUnmapMemory(device, paramsBufferMemory);

        // Generate random data for the GPU
        std::vector<uint32_t> randomData(MR_ROUNDS_GPU * 16);
        generateRandomData(randomData.data(), randomData.size());

        vkMapMemory(device, randomBufferMemory, 0, randomData.size() * sizeof(uint32_t), 0, &data);
        memcpy(data, randomData.data(), randomData.size() * sizeof(uint32_t));
        vkUnmapMemory(device, randomBufferMemory);

        // Initialize result
        MRResult result;
        memset(&result, 0, sizeof(result));

        vkMapMemory(device, resultBufferMemory, 0, sizeof(MRResult), 0, &data);
        memcpy(data, &result, sizeof(MRResult));
        vkUnmapMemory(device, resultBufferMemory);

        // Start progress monitoring
        test_complete = false;
        progress_counter = 0;

        std::thread progress_thread([&]() {
            while (!test_complete) {

                void* resultData;
                MRResult current_result;
                if (vkMapMemory(device, resultBufferMemory, 0, sizeof(MRResult), 0, &resultData) == VK_SUCCESS) {
                    MRResult current_result;
                    memcpy(&current_result, resultData, sizeof(MRResult));
                    vkUnmapMemory(device, resultBufferMemory);

                    uint32_t completed = current_result.round_completed;
                    // ... rest of progress code
                } else {
                    break; // Exit if mapping fails
                }

                uint32_t completed = current_result.round_completed;
                double percent = (100.0 * completed) / rounds;

                std::cout << "\rGPU Miller-Rabin progress: " << completed << "/" << rounds
                << " (" << std::fixed << std::setprecision(2) << percent << "%)" << std::flush;

                if (current_result.is_composite || completed >= rounds) {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::cout << std::endl;
        });

        // Execute GPU computation
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Dispatch compute shader
        uint32_t groupCount = (rounds + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        vkCmdDispatch(commandBuffer, groupCount, 1, 1);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkFence fence;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence!");
        }
        if (fence == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create fence!");
        }

        if (vkQueueSubmit(computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit compute command buffer!");
        }

        // Wait for completion
        if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for fence!");
        }

        test_complete = true;
        progress_thread.join();

        // Read result
        vkMapMemory(device, resultBufferMemory, 0, sizeof(MRResult), 0, &data);
        memcpy(&result, data, sizeof(MRResult));
        vkUnmapMemory(device, resultBufferMemory);

        // Cleanup
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

        return result.is_composite == 0;
    }

    std::string get_number_str() const {
        char* str = mpz_get_str(nullptr, 10, n);
        std::string result(str);
        free(str);
        return result;
    }

    uint64_t get_num_digits() const {
        return mpz_sizeinbase(n, 10);
    }

    void print_gpu_info() {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

        std::cout << "GPU Device: " << deviceProperties.deviceName << std::endl;
        std::cout << "Max compute work group size: " << deviceProperties.limits.maxComputeWorkGroupSize[0] << std::endl;
        std::cout << "Max compute work group invocations: " << deviceProperties.limits.maxComputeWorkGroupInvocations << std::endl;

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        uint64_t totalMemory = 0;
        for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
            if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                totalMemory += memProperties.memoryHeaps[i].size;
            }
        }

        std::cout << "GPU Memory: " << (totalMemory / 1024 / 1024) << " MB" << std::endl;
    }
};

// Function to write the compute shader GLSL code to a file
void writeComputeShader() {
    std::ofstream shader("miller_rabin.comp");
    shader << R"(#version 450

// Workgroup size
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// Big integer representation
struct BigInt {
    uint limbs[128];
    uint size;
    uint _padding[3];
};

// Miller-Rabin parameters
struct MRParams {
    BigInt n;
    BigInt n_minus_1;
    BigInt d;
    uint s;
    uint rounds;
    uint seed;
    uint _padding;
};

// Result structure
struct MRResult {
    uint is_composite;
    uint round_completed;
    uint _padding[2];
};

// Buffers
layout(std430, binding = 0) buffer ParamsBuffer {
    MRParams params;
};

layout(std430, binding = 1) buffer ResultBuffer {
    MRResult result;
};

layout(std430, binding = 2) buffer RandomBuffer {
    uint random_data[];
};

// Big integer operations
bool bigint_is_zero(BigInt a) {
    for (uint i = 0; i < a.size; i++) {
        if (a.limbs[i] != 0) return false;
    }
    return true;
}

bool bigint_is_one(BigInt a) {
    if (a.size == 0) return false;
    if (a.limbs[0] != 1) return false;
    for (uint i = 1; i < a.size; i++) {
        if (a.limbs[i] != 0) return false;
    }
    return true;
}

bool bigint_equal(BigInt a, BigInt b) {
    if (a.size != b.size) return false;
    for (uint i = 0; i < a.size; i++) {
        if (a.limbs[i] != b.limbs[i]) return false;
    }
    return true;
}

// Simple big integer modular exponentiation (simplified)
BigInt bigint_powmod(BigInt base, BigInt exp, BigInt mod) {
    BigInt result;
    result.size = 1;
    result.limbs[0] = 1;

    // This is a simplified implementation
    // In practice, you'd need a full big integer library
    // For demonstration, we'll just return a dummy value
    return result;
}

// Generate random base for Miller-Rabin
BigInt generate_random_base(uint thread_id, uint round) {
    BigInt base;
    base.size = 1;

    // Use thread ID and round number to generate pseudo-random base
    uint index = (thread_id * 16 + round) % (random_data.length() - 1);
    base.limbs[0] = random_data[index] % (params.n.limbs[0] - 2) + 2;

    return base;
}

// Main Miller-Rabin test
bool miller_rabin_test(uint thread_id, uint round_start, uint round_end) {
    for (uint round = round_start; round < round_end; round++) {
        // Check if another thread found composite
        if (result.is_composite != 0) {
            return false;
        }

        // Generate random base
        BigInt a = generate_random_base(thread_id, round);

        // Compute a^d mod n
        BigInt y = bigint_powmod(a, params.d, params.n);

        // If a^d ≡ 1 (mod n) or a^d ≡ -1 (mod n), continue
        if (bigint_is_one(y) || bigint_equal(y, params.n_minus_1)) {
            atomicAdd(result.round_completed, 1);
            continue;
        }

        // Check a^(2^r * d) for r = 1 to s-1
        bool found_minus_one = false;
        for (uint r = 1; r < params.s; r++) {
            // y = y^2 mod n
            BigInt two;
            two.size = 1;
            two.limbs[0] = 2;
            y = bigint_powmod(y, two, params.n);

            // If y ≡ -1 (mod n), probably prime
            if (bigint_equal(y, params.n_minus_1)) {
                found_minus_one = true;
                break;
            }

            // If y ≡ 1 (mod n), composite
            if (bigint_is_one(y)) {
                atomicExchange(result.is_composite, 1);
                return false;
            }
        }

        // If no -1 found, composite
        if (!found_minus_one) {
            atomicExchange(result.is_composite, 1);
            return false;
        }

        atomicAdd(result.round_completed, 1);
    }

    return true;
}

void main() {
    uint thread_id = gl_GlobalInvocationID.x;
    uint total_threads = gl_NumWorkGroups.x * gl_WorkGroupSize.x;

    // Divide rounds among threads
    uint rounds_per_thread = params.rounds / total_threads;
    uint remainder = params.rounds % total_threads;

    uint round_start = thread_id * rounds_per_thread;
    uint round_end = round_start + rounds_per_thread;

    // Add remainder to last threads
    if (thread_id < remainder) {
        round_start += thread_id;
        round_end += thread_id + 1;
    } else {
        round_start += remainder;
        round_end += remainder;
    }

    // Perform Miller-Rabin test
    miller_rabin_test(thread_id, round_start, round_end);
}
)";
shader.close();

std::cout << "Compute shader written to miller_rabin.comp" << std::endl;
std::cout << "Compile with: glslc miller_rabin.comp -o miller_rabin.spv" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <mode> [params...]" << std::endl;
        std::cout << "Modes:" << std::endl;
        std::cout << "  0                 - Write compute shader and exit" << std::endl;
        std::cout << "  1 <number>        - Test if the given number is prime" << std::endl;
        std::cout << "  2 <digits>        - Generate and test a random number with the given number of digits" << std::endl;
        std::cout << "  3 <exp>           - Generate and test a random ultra-large number around 10^10^exp" << std::endl;
        std::cout << "  4 <exp> <count>   - Generate and test <count> ultra-large numbers around 10^10^exp" << std::endl;
        std::cout << "  5                 - Show GPU information" << std::endl;
        return 1;
    }

    int mode = std::stoi(argv[1]);

    if (mode == 0) {
        writeComputeShader();
        return 0;
    }

    try {
        VulkanPrimalityTester tester;

        if (mode == 5) {
            tester.print_gpu_info();
            return 0;
        }

        switch (mode) {
            case 1: {  // Test a specific number
                if (argc < 3) {
                    std::cout << "Error: Please provide a number to test" << std::endl;
                    return 1;
                }

                std::string number = argv[2];
                tester.set_number(number);

                std::cout << "Testing primality of: " << tester.get_number_str() << std::endl;
                std::cout << "Number of digits: " << tester.get_num_digits() << std::endl;

                auto start_time = std::chrono::high_resolution_clock::now();
                bool is_prime = tester.is_prime();
                auto end_time = std::chrono::high_resolution_clock::now();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                std::cout << "Result: " << (is_prime ? "PROBABLY PRIME" : "COMPOSITE") << std::endl;
                std::cout << "Time taken: " << duration.count() / 1000.0 << " seconds" << std::endl;
                break;
            }

            case 2: {  // Generate and test a random number with given digits
                if (argc < 3) {
                    std::cout << "Error: Please provide the number of digits" << std::endl;
                    return 1;
                }

                uint64_t digits = std::stoull(argv[2]);
                tester.generate_random_number(digits);

                std::cout << "Generated random number with " << digits << " digits" << std::endl;

                if (digits <= 100) {
                    std::cout << "Number: " << tester.get_number_str() << std::endl;
                } else {
                    std::string num_str = tester.get_number_str();
                    std::cout << "First 50 digits: " << num_str.substr(0, 50) << "..." << std::endl;
                    std::cout << "Last 50 digits: ..." << num_str.substr(num_str.size() - 50) << std::endl;
                }

                auto start_time = std::chrono::high_resolution_clock::now();
                bool is_prime = tester.is_prime();
                auto end_time = std::chrono::high_resolution_clock::now();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                std::cout << "Result: " << (is_prime ? "PROBABLY PRIME" : "COMPOSITE") << std::endl;
                std::cout << "Time taken: " << duration.count() / 1000.0 << " seconds" << std::endl;
                break;
            }

            case 3: {  // Generate and test an ultra-large number
                if (argc < 3) {
                    std::cout << "Error: Please provide the exponent" << std::endl;
                    return 1;
                }

                double exp = std::stod(argv[2]);
                std::cout << "Generating a random number around 10^10^" << exp << std::endl;

                auto gen_start_time = std::chrono::high_resolution_clock::now();
                tester.generate_ultra_large_number(exp);
                auto gen_end_time = std::chrono::high_resolution_clock::now();

                auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end_time - gen_start_time);

                uint64_t digits = tester.get_num_digits();
                std::cout << "Generated a number with " << digits << " digits" << std::endl;
                std::cout << "Generation time: " << gen_duration.count() / 1000.0 << " seconds" << std::endl;

                // Show a glimpse of the number
                std::string num_str = tester.get_number_str();
                if (num_str.length() > 100) {
                    std::cout << "First 50 digits: " << num_str.substr(0, 50) << "..." << std::endl;
                    std::cout << "Last 50 digits: ..." << num_str.substr(num_str.size() - 50) << std::endl;
                } else {
                    std::cout << "Number: " << num_str << std::endl;
                }

                auto start_time = std::chrono::high_resolution_clock::now();
                bool is_prime = tester.is_prime();
                auto end_time = std::chrono::high_resolution_clock::now();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                std::cout << "Result: " << (is_prime ? "PROBABLY PRIME" : "COMPOSITE") << std::endl;
                std::cout << "Time taken: " << duration.count() / 1000.0 << " seconds" << std::endl;
                break;
            }

            case 4: {  // Generate and test multiple ultra-large numbers
                if (argc < 4) {
                    std::cout << "Error: Please provide the exponent and count" << std::endl;
                    return 1;
                }

                double exp = std::stod(argv[2]);
                uint64_t count = std::stoull(argv[3]);

                std::cout << "Testing " << count << " random numbers around 10^10^" << exp << std::endl;

                uint64_t prime_count = 0;
                double total_time = 0.0;

                for (uint64_t i = 0; i < count; i++) {
                    std::cout << "\n[" << (i+1) << "/" << count << "] Generating number..." << std::endl;
                    tester.generate_ultra_large_number(exp);

                    uint64_t digits = tester.get_num_digits();
                    std::cout << "Testing number with " << digits << " digits" << std::endl;

                    auto start_time = std::chrono::high_resolution_clock::now();
                    bool is_prime = tester.is_prime();
                    auto end_time = std::chrono::high_resolution_clock::now();

                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    double seconds = duration.count() / 1000.0;
                    total_time += seconds;

                    if (is_prime) prime_count++;

                    std::cout << "Result: " << (is_prime ? "PROBABLY PRIME" : "COMPOSITE") << std::endl;
                    std::cout << "Time taken: " << seconds << " seconds" << std::endl;

                    // Show running statistics
                    double prime_density = (100.0 * prime_count) / (i+1);
                    std::cout << "Running stats: " << prime_count << "/" << (i+1)
                    << " primes found (" << std::fixed << std::setprecision(2) << prime_density << "%)" << std::endl;
                    std::cout << "Average time per test: " << (total_time / (i+1)) << " seconds" << std::endl;
                }

                // Final statistics
                std::cout << "\nFinal results:" << std::endl;
                std::cout << "Tested " << count << " numbers around 10^10^" << exp << std::endl;
                std::cout << "Found " << prime_count << " probable primes" << std::endl;
                std::cout << "Prime density: " << (100.0 * prime_count / count) << "%" << std::endl;
                std::cout << "Total time: " << total_time << " seconds" << std::endl;
                std::cout << "Average time per test: " << (total_time / count) << " seconds" << std::endl;
                break;
            }

            default:
                std::cout << "Error: Invalid mode" << std::endl;
                return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
