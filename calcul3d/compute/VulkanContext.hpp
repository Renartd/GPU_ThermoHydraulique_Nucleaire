#pragma once
// ============================================================
//  VulkanContext.hpp  v2.0
//
//  Contexte Vulkan minimal pour le simulateur.
//  Fournit : instance, device, queue, cmdPool, descriptorPool
//
//  AJOUTS v2 :
//    - uploadToDeviceLocal(buffer, data, size) : staging auto
//    - createComputePipeline(spvPath, layout, shaderModule)
//    - descriptorPool agrandi pour 17 bindings × 2 sets FVM + reduce
// ============================================================
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>

#define VK_CHECK_CTX(call) do { \
    VkResult _r=(call); \
    if(_r!=VK_SUCCESS) \
        throw std::runtime_error(std::string("[VkCtx] ")+#call+" = "+std::to_string(_r)); \
} while(0)

struct VulkanContext {
    VkInstance       instance       = VK_NULL_HANDLE;
    VkPhysicalDevice physDev        = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          computeQueue   = VK_NULL_HANDLE;
    VkCommandPool    cmdPool        = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    uint32_t         queueFamily    = 0;

    VkPhysicalDeviceMemoryProperties memProps{};

    // ── Initialisation complète ──────────────────────────────
    void init() {
        _createInstance();
        _pickPhysicalDevice();
        _createDevice();
        _createCommandPool();
        _createDescriptorPool();
        vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
        std::cout << "[VulkanContext] Init OK\n";
    }

    // ── Nettoyage ────────────────────────────────────────────
    void cleanup() {
        if (descriptorPool) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }
        if (cmdPool) {
            vkDestroyCommandPool(device, cmdPool, nullptr);
            cmdPool = VK_NULL_HANDLE;
        }
        if (device) { vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE; }
        if (instance){ vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
    }

    // ── Allocation buffer ────────────────────────────────────
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags memFlags,
                      VkBuffer& outBuf, VkDeviceMemory& outMem)
    {
        VkBufferCreateInfo bCI{};
        bCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bCI.size        = size;
        bCI.usage       = usage;
        bCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK_CTX(vkCreateBuffer(device, &bCI, nullptr, &outBuf));

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, outBuf, &req);

        VkMemoryAllocateInfo allocCI{};
        allocCI.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocCI.allocationSize  = req.size;
        allocCI.memoryTypeIndex = _findMemType(req.memoryTypeBits, memFlags);
        VK_CHECK_CTX(vkAllocateMemory(device, &allocCI, nullptr, &outMem));
        VK_CHECK_CTX(vkBindBufferMemory(device, outBuf, outMem, 0));
    }

    // ── Upload vers DEVICE_LOCAL via staging buffer ───────────
    void uploadToDeviceLocal(VkBuffer dst, const void* data, VkDeviceSize size) {
        VkBuffer stageBuf; VkDeviceMemory stageMem;
        createBuffer(size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stageBuf, stageMem);

        void* ptr;
        vkMapMemory(device, stageMem, 0, size, 0, &ptr);
        memcpy(ptr, data, size);
        vkUnmapMemory(device, stageMem);

        auto cb = beginOneShot();
        VkBufferCopy reg{0,0,size};
        vkCmdCopyBuffer(cb, stageBuf, dst, 1, &reg);
        endOneShot(cb);

        vkDestroyBuffer(device, stageBuf, nullptr);
        vkFreeMemory(device, stageMem, nullptr);
    }

    // ── Création pipeline compute depuis .spv ─────────────────
    VkPipeline createComputePipeline(const std::string& spvPath,
                                     VkPipelineLayout layout,
                                     VkShaderModule& outModule)
    {
        // Lecture du .spv
        std::ifstream f(spvPath, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            throw std::runtime_error("[VkCtx] Shader introuvable : " + spvPath);
        size_t sz = f.tellg(); f.seekg(0);
        std::vector<uint32_t> code(sz / 4);
        f.read(reinterpret_cast<char*>(code.data()), sz);

        VkShaderModuleCreateInfo smCI{};
        smCI.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smCI.codeSize = sz;
        smCI.pCode    = code.data();
        VK_CHECK_CTX(vkCreateShaderModule(device, &smCI, nullptr, &outModule));

        VkPipelineShaderStageCreateInfo stageCI{};
        stageCI.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stageCI.module = outModule;
        stageCI.pName  = "main";

        VkComputePipelineCreateInfo cpCI{};
        cpCI.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpCI.stage  = stageCI;
        cpCI.layout = layout;

        VkPipeline pipeline;
        VK_CHECK_CTX(vkCreateComputePipelines(device, VK_NULL_HANDLE,
                                              1, &cpCI, nullptr, &pipeline));
        return pipeline;
    }

    // ── Command buffer one-shot ───────────────────────────────
    VkCommandBuffer beginOneShot() {
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = cmdPool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VkCommandBuffer cb;
        VK_CHECK_CTX(vkAllocateCommandBuffers(device, &ai, &cb));

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK_CTX(vkBeginCommandBuffer(cb, &bi));
        return cb;
    }

    void endOneShot(VkCommandBuffer cb) {
        VK_CHECK_CTX(vkEndCommandBuffer(cb));

        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cb;
        VK_CHECK_CTX(vkQueueSubmit(computeQueue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK_CTX(vkQueueWaitIdle(computeQueue));
        vkFreeCommandBuffers(device, cmdPool, 1, &cb);
    }

private:

    // ───────────────────────────────────────────────────────────────
    //  INSTANCE
    // ───────────────────────────────────────────────────────────────
    void _createInstance() {
        VkApplicationInfo app{};
        app.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &app;

#ifndef NDEBUG
        const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
        ci.enabledLayerCount   = 1;
        ci.ppEnabledLayerNames = layers;
#endif
        VK_CHECK_CTX(vkCreateInstance(&ci, nullptr, &instance));
    }

    // ───────────────────────────────────────────────────────────────
    //  GPU SELECTION — VERSION ROBUSTE (NVIDIA > Intel > llvmpipe)
    // ───────────────────────────────────────────────────────────────
    void _pickPhysicalDevice() {
        uint32_t cnt = 0;
        vkEnumeratePhysicalDevices(instance, &cnt, nullptr);
        if (!cnt) throw std::runtime_error("[VkCtx] Aucun GPU Vulkan");
        std::vector<VkPhysicalDevice> devs(cnt);
        vkEnumeratePhysicalDevices(instance, &cnt, devs.data());

        VkPhysicalDevice best = VK_NULL_HANDLE;
        int bestScore = -1;

        for (auto d : devs) {
            VkPhysicalDeviceProperties p;
            vkGetPhysicalDeviceProperties(d, &p);

            int score = 0;

            if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                score = 1000;
            else if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                score = 500;
            else
                score = 10;

            if (score > bestScore) {
                bestScore = score;
                best = d;
            }
        }

        if (best == VK_NULL_HANDLE)
            throw std::runtime_error("[VkCtx] Aucun GPU compatible trouvé !");

        physDev = best;

        VkPhysicalDeviceProperties pp;
        vkGetPhysicalDeviceProperties(physDev, &pp);
        std::cout << "[VkCtx] GPU sélectionné : " << pp.deviceName << "\n";
    }

    // ───────────────────────────────────────────────────────────────
    //  DEVICE + QUEUE
    // ───────────────────────────────────────────────────────────────
    void _createDevice() {
        uint32_t qfCnt=0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDev, &qfCnt, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qfCnt);
        vkGetPhysicalDeviceQueueFamilyProperties(physDev, &qfCnt, qf.data());

        queueFamily = UINT32_MAX;
        for (uint32_t i=0;i<qfCnt;i++)
            if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                queueFamily=i; break;
            }
        if (queueFamily==UINT32_MAX)
            throw std::runtime_error("[VkCtx] Pas de queue COMPUTE");

        float prio=1.0f;
        VkDeviceQueueCreateInfo qCI{};
        qCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qCI.queueFamilyIndex = queueFamily;
        qCI.queueCount       = 1;
        qCI.pQueuePriorities = &prio;

        VkDeviceCreateInfo dCI{};
        dCI.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dCI.queueCreateInfoCount = 1;
        dCI.pQueueCreateInfos    = &qCI;
        VK_CHECK_CTX(vkCreateDevice(physDev, &dCI, nullptr, &device));
        vkGetDeviceQueue(device, queueFamily, 0, &computeQueue);
    }

    // ───────────────────────────────────────────────────────────────
    //  COMMAND POOL
    // ───────────────────────────────────────────────────────────────
    void _createCommandPool() {
        VkCommandPoolCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = queueFamily;
        ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK_CTX(vkCreateCommandPool(device, &ci, nullptr, &cmdPool));
    }

    // ───────────────────────────────────────────────────────────────
    //  DESCRIPTOR POOL
    // ───────────────────────────────────────────────────────────────
    void _createDescriptorPool() {
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 200 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  40 },
        };
        VkDescriptorPoolCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        ci.maxSets       = 40;
        ci.poolSizeCount = 2;
        ci.pPoolSizes    = sizes;
        VK_CHECK_CTX(vkCreateDescriptorPool(device, &ci, nullptr, &descriptorPool));
    }

    void resetDescriptorPool() {
        if (descriptorPool)
            vkResetDescriptorPool(device, descriptorPool, 0);
    }

    uint32_t _findMemType(uint32_t typeBits, VkMemoryPropertyFlags flags) {
        for (uint32_t i=0;i<memProps.memoryTypeCount;i++)
            if ((typeBits&(1u<<i)) &&
                (memProps.memoryTypes[i].propertyFlags&flags)==flags)
                return i;
        throw std::runtime_error("[VkCtx] Type mémoire introuvable");
    }
};
