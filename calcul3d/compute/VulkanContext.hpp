#pragma once
// ============================================================
//  VulkanContext.hpp
//  Initialise le device Vulkan avec les extensions RT requises
//  Extensions nécessaires :
//    VK_KHR_acceleration_structure
//    VK_KHR_ray_query
//    VK_KHR_deferred_host_operations
//    VK_KHR_spirv_1_4
//    VK_KHR_shader_float_controls
// ============================================================

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

// ============================================================
//  Macro utilitaire
// ============================================================
#define VK_CHECK(call) \
    do { \
        VkResult _r = (call); \
        if (_r != VK_SUCCESS) { \
            std::cerr << "[Vulkan] Erreur " << _r \
                      << " dans " << #call \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            throw std::runtime_error("Vulkan error"); \
        } \
    } while(0)

// ============================================================
//  Extensions requises
// ============================================================
static const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
};

// ============================================================
//  Structure principale du contexte Vulkan
// ============================================================
struct VulkanContext {

    VkInstance               instance       = VK_NULL_HANDLE;
    VkPhysicalDevice         physDevice     = VK_NULL_HANDLE;
    VkDevice                 device         = VK_NULL_HANDLE;
    VkQueue                  computeQueue   = VK_NULL_HANDLE;
    uint32_t                 computeFamily  = 0;
    VkCommandPool            cmdPool        = VK_NULL_HANDLE;
    VkDescriptorPool         descPool       = VK_NULL_HANDLE;

    // Propriétés RT
    VkPhysicalDeviceRayQueryFeaturesKHR           rayQueryFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};

    bool rtSupported = false;

    // --------------------------------------------------------
    //  Initialisation complète
    // --------------------------------------------------------
    bool init() {
        try {
            createInstance();
            pickPhysicalDevice();
            createDevice();
            createCommandPool();
            createDescriptorPool();
            rtSupported = true;
            std::cout << "[VulkanContext] RT initialisé avec succès\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[VulkanContext] Echec init : " << e.what() << "\n";
            rtSupported = false;
            return false;
        }
    }

    void cleanup() {
        if (descPool)  vkDestroyDescriptorPool(device, descPool, nullptr);
        if (cmdPool)   vkDestroyCommandPool(device, cmdPool, nullptr);
        if (device)    vkDestroyDevice(device, nullptr);
        if (instance)  vkDestroyInstance(instance, nullptr);
    }

    // --------------------------------------------------------
    //  Utilitaire : trouver le type de mémoire
    // --------------------------------------------------------
    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags props) const {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((filter & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        throw std::runtime_error("Aucun type mémoire compatible trouvé");
    }

    // --------------------------------------------------------
    //  Utilitaire : créer un buffer
    // --------------------------------------------------------
    void createBuffer(VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props,
                      VkBuffer& buf,
                      VkDeviceMemory& mem) const {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = size;
        bi.usage       = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &buf));

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, buf, &req);

        VkMemoryAllocateFlagsInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.pNext           = &flagsInfo;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
        VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &mem));
        VK_CHECK(vkBindBufferMemory(device, buf, mem, 0));
    }

    // --------------------------------------------------------
    //  Utilitaire : command buffer one-shot
    // --------------------------------------------------------
    VkCommandBuffer beginOneShot() const {
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = cmdPool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VkCommandBuffer cb;
        VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cb));
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cb, &bi));
        return cb;
    }

    void endOneShot(VkCommandBuffer cb) const {
        VK_CHECK(vkEndCommandBuffer(cb));
        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cb;
        VK_CHECK(vkQueueSubmit(computeQueue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(computeQueue));
        vkFreeCommandBuffers(device, cmdPool, 1, &cb);
    }

private:

    // --------------------------------------------------------
    void createInstance() {
        VkApplicationInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        ai.pApplicationName   = "calcul3d";
        ai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        ai.apiVersion         = VK_API_VERSION_1_3;

        // Extensions instance
        std::vector<const char*> instExts = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
        };

        VkInstanceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo        = &ai;
        ci.enabledExtensionCount   = (uint32_t)instExts.size();
        ci.ppEnabledExtensionNames = instExts.data();
        VK_CHECK(vkCreateInstance(&ci, nullptr, &instance));
    }

    // --------------------------------------------------------
    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) throw std::runtime_error("Aucun GPU Vulkan trouvé");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());

        // Préférer le GPU intégré AMD
        for (auto& d : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(d, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                physDevice = d;
                std::cout << "[VulkanContext] GPU : " << props.deviceName << "\n";
                return;
            }
        }
        // Fallback : premier GPU
        physDevice = devices[0];
    }

    // --------------------------------------------------------
    void createDevice() {
        // Trouver la queue compute
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &qCount, qProps.data());

        computeFamily = UINT32_MAX;
        for (uint32_t i = 0; i < qCount; ++i) {
            if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                computeFamily = i;
                break;
            }
        }
        if (computeFamily == UINT32_MAX)
            throw std::runtime_error("Pas de queue compute");

        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = computeFamily;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &prio;

        // Chaîne de features RT
        VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
        bdaFeatures.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bdaFeatures.bufferDeviceAddress = VK_TRUE;

        accelFeatures.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelFeatures.accelerationStructure = VK_TRUE;
        accelFeatures.pNext                 = &bdaFeatures;

        rayQueryFeatures.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        rayQueryFeatures.rayQuery = VK_TRUE;
        rayQueryFeatures.pNext    = &accelFeatures;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &rayQueryFeatures;

        VkDeviceCreateInfo dci{};
        dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.pNext                   = &features2;
        dci.queueCreateInfoCount    = 1;
        dci.pQueueCreateInfos       = &qci;
        dci.enabledExtensionCount   = (uint32_t)DEVICE_EXTENSIONS.size();
        dci.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
        VK_CHECK(vkCreateDevice(physDevice, &dci, nullptr, &device));

        vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
    }

    // --------------------------------------------------------
    void createCommandPool() {
        VkCommandPoolCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = computeFamily;
        ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(device, &ci, nullptr, &cmdPool));
    }

    // --------------------------------------------------------
    void createDescriptorPool() {
        std::vector<VkDescriptorPoolSize> sizes = {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 4},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             64},  // ThermalCompute+NeutronCompute+marge
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              4},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             16},
        };
        VkDescriptorPoolCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.maxSets       = 32;
        ci.poolSizeCount = (uint32_t)sizes.size();
        ci.pPoolSizes    = sizes.data();
        VK_CHECK(vkCreateDescriptorPool(device, &ci, nullptr, &descPool));
    }
};