#pragma once
// ============================================================
//  ShadowCompute.hpp
//  Pipeline Vulkan Compute + Ray Query pour calcul des ombres
//  Résultat : std::vector<float> shadowFactors[]
//             0.0 = ombre, 1.0 = lumière
// ============================================================

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cmath>
#include "VulkanContext.hpp"
#include "AccelStructure.hpp"
#include "../core/GridData.hpp"

struct LightParams {
    float lightDir[4]   = {1.0f, 0.3f, 0.5f, 0.0f}; // direction lumière
    float lightColor[4] = {1.0f, 1.0f, 0.9f, 1.0f};
    int   assemblyCount = 0;
    int   numSamples    = 16;    // 8 rayons = ombres douces correctes
    float shadowBias    = 0.01f;
    float lightRadius   = 0.15f; // dispersion pour soft shadows
};

class ShadowCompute {
public:
    bool ready = false;

    std::vector<float> shadowFactors; // résultat lu par le renderer

    // --------------------------------------------------------
    bool init(VulkanContext& ctx, const GridData& grid) {
        try {
            _ctx = &ctx;
            _assemblyCount = (int)grid.cubes.size();
            shadowFactors.assign(_assemblyCount, 1.0f);

            // 1. Construit la BVH
            _accel.build(ctx, grid);

            // 2. Crée les buffers GPU
            createBuffers(grid);

            // 3. Charge et compile le shader SPIR-V
            createPipeline();

            // 4. Crée les descriptors
            createDescriptors();

            ready = true;
            std::cout << "[ShadowCompute] Prêt (" << _assemblyCount << " assemblages)\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[ShadowCompute] Echec init : " << e.what() << "\n";
            return false;
        }
    }

    // --------------------------------------------------------
    //  Lance le calcul RT sur GPU
    //  Appelez chaque frame ou quand la lumière change
    // --------------------------------------------------------
    void compute(LightParams& params) {
        if (!ready) return;

        params.assemblyCount = _assemblyCount;

        // Met à jour l'UBO params
        void* ptr;
        vkMapMemory(_ctx->device, _paramsMem, 0, sizeof(LightParams), 0, &ptr);
        memcpy(ptr, &params, sizeof(LightParams));
        vkUnmapMemory(_ctx->device, _paramsMem);

        // Lance le compute shader
        VkCommandBuffer cb = _ctx->beginOneShot();

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _pipelineLayout, 0, 1, &_descSet, 0, nullptr);

        // Dispatch : 1 thread par assemblage (groupes de 64)
        uint32_t groups = (_assemblyCount + 63) / 64;
        vkCmdDispatch(cb, groups, 1, 1);

        _ctx->endOneShot(cb);

        // Rapatrie les résultats en CPU
        readbackShadows();
    }

    // Modifie la direction lumière
    void setLightDir(float x, float y, float z, LightParams& params) {
        float len = sqrtf(x*x + y*y + z*z);
        if (len < 0.001f) return;
        params.lightDir[0] = x/len;
        params.lightDir[1] = y/len;
        params.lightDir[2] = z/len;
    }

    void cleanup() {
        if (!_ctx) return;
        _accel.cleanup(*_ctx);
        auto destroy = [&](VkBuffer b, VkDeviceMemory m) {
            if (b) vkDestroyBuffer(_ctx->device, b, nullptr);
            if (m) vkFreeMemory(_ctx->device, m, nullptr);
        };
        destroy(_shadowBuf,   _shadowMem);
        destroy(_shadowRBuf,  _shadowRMem);
        destroy(_paramsBuf,   _paramsMem);
        destroy(_posBuf,      _posMem);
        if (_pipeline)       vkDestroyPipeline(_ctx->device, _pipeline, nullptr);
        if (_pipelineLayout) vkDestroyPipelineLayout(_ctx->device, _pipelineLayout, nullptr);
        if (_descSetLayout)  vkDestroyDescriptorSetLayout(_ctx->device, _descSetLayout, nullptr);
        if (_shaderModule)   vkDestroyShaderModule(_ctx->device, _shaderModule, nullptr);
    }

private:
    VulkanContext* _ctx = nullptr;
    AccelStructure _accel;
    int _assemblyCount = 0;

    // Buffers GPU
    VkBuffer       _shadowBuf  = VK_NULL_HANDLE; // shadow output (device local)
    VkDeviceMemory _shadowMem  = VK_NULL_HANDLE;
    VkBuffer       _shadowRBuf = VK_NULL_HANDLE; // readback (host visible)
    VkDeviceMemory _shadowRMem = VK_NULL_HANDLE;
    VkBuffer       _paramsBuf  = VK_NULL_HANDLE;
    VkDeviceMemory _paramsMem  = VK_NULL_HANDLE;
    VkBuffer       _posBuf     = VK_NULL_HANDLE;
    VkDeviceMemory _posMem     = VK_NULL_HANDLE;

    // Pipeline
    VkShaderModule        _shaderModule   = VK_NULL_HANDLE;
    VkPipelineLayout      _pipelineLayout = VK_NULL_HANDLE;
    VkPipeline            _pipeline       = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descSetLayout  = VK_NULL_HANDLE;
    VkDescriptorSet       _descSet        = VK_NULL_HANDLE;

    // --------------------------------------------------------
    void createBuffers(const GridData& grid) {
        VkDeviceSize shadowSize = _assemblyCount * sizeof(float);
        VkDeviceSize posSize    = _assemblyCount * sizeof(float) * 4;

        // Buffer shadow output (device local)
        _ctx->createBuffer(shadowSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _shadowBuf, _shadowMem);

        // Buffer readback (host visible)
        _ctx->createBuffer(shadowSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _shadowRBuf, _shadowRMem);

        // UBO params
        _ctx->createBuffer(sizeof(LightParams),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _paramsBuf, _paramsMem);

        // Buffer positions assemblages
        _ctx->createBuffer(posSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _posBuf, _posMem);

        // Upload positions
        std::vector<float> pos;
        pos.reserve(_assemblyCount * 4);
        for (const auto& c : grid.cubes) {
            pos.push_back(c.pos.x);
            pos.push_back(0.0f);
            pos.push_back(c.pos.z);
            pos.push_back(0.0f);
        }
        void* ptr;
        vkMapMemory(_ctx->device, _posMem, 0, posSize, 0, &ptr);
        memcpy(ptr, pos.data(), posSize);
        vkUnmapMemory(_ctx->device, _posMem);
    }

    // --------------------------------------------------------
    void createPipeline() {
        // Charge le SPIR-V compilé
        auto spv = loadSPV("compute/shaders/shadow_rq.spv");

        VkShaderModuleCreateInfo smci{};
        smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = spv.size() * sizeof(uint32_t);
        smci.pCode    = spv.data();
        VK_CHECK(vkCreateShaderModule(_ctx->device, &smci, nullptr, &_shaderModule));

        // Layout descriptors
        VkDescriptorSetLayoutBinding bindings[4] = {};
        // binding 0 : TLAS
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        // binding 1 : shadow buffer
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        // binding 2 : params UBO
        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        // binding 3 : positions
        bindings[3].binding         = 3;
        bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo dslci{};
        dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 4;
        dslci.pBindings    = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(_ctx->device, &dslci, nullptr, &_descSetLayout));

        VkPipelineLayoutCreateInfo plci{};
        plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts    = &_descSetLayout;
        VK_CHECK(vkCreatePipelineLayout(_ctx->device, &plci, nullptr, &_pipelineLayout));

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = _shaderModule;
        stage.pName  = "main";

        VkComputePipelineCreateInfo cpci{};
        cpci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpci.stage  = stage;
        cpci.layout = _pipelineLayout;
        VK_CHECK(vkCreateComputePipelines(_ctx->device, VK_NULL_HANDLE,
                                          1, &cpci, nullptr, &_pipeline));
    }

    // --------------------------------------------------------
    void createDescriptors() {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = _ctx->descPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &_descSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(_ctx->device, &ai, &_descSet));

        // Write 0 : TLAS
        VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
        asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asWrite.accelerationStructureCount = 1;
        asWrite.pAccelerationStructures    = &_accel.tlas;

        // Write 1 : shadow buffer
        VkDescriptorBufferInfo shadowInfo{};
        shadowInfo.buffer = _shadowBuf;
        shadowInfo.range  = VK_WHOLE_SIZE;

        // Write 2 : params
        VkDescriptorBufferInfo paramsInfo{};
        paramsInfo.buffer = _paramsBuf;
        paramsInfo.range  = VK_WHOLE_SIZE;

        // Write 3 : positions
        VkDescriptorBufferInfo posInfo{};
        posInfo.buffer = _posBuf;
        posInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writes[4] = {};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].pNext           = &asWrite;
        writes[0].dstSet          = _descSet;
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = _descSet;
        writes[1].dstBinding      = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo     = &shadowInfo;

        writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet          = _descSet;
        writes[2].dstBinding      = 2;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].pBufferInfo     = &paramsInfo;

        writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet          = _descSet;
        writes[3].dstBinding      = 3;
        writes[3].descriptorCount = 1;
        writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[3].pBufferInfo     = &posInfo;

        vkUpdateDescriptorSets(_ctx->device, 4, writes, 0, nullptr);
    }

    // --------------------------------------------------------
    void readbackShadows() {
        VkDeviceSize size = _assemblyCount * sizeof(float);

        // Copie device → host
        VkCommandBuffer cb = _ctx->beginOneShot();
        VkBufferCopy region{0, 0, size};
        vkCmdCopyBuffer(cb, _shadowBuf, _shadowRBuf, 1, &region);
        _ctx->endOneShot(cb);

        void* ptr;
        vkMapMemory(_ctx->device, _shadowRMem, 0, size, 0, &ptr);
        memcpy(shadowFactors.data(), ptr, size);
        vkUnmapMemory(_ctx->device, _shadowRMem);
    }

    // --------------------------------------------------------
    std::vector<uint32_t> loadSPV(const std::string& path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            throw std::runtime_error("Shader SPIR-V introuvable : " + path +
                "\n  → lancez d'abord : ./compile_shaders.sh");
        size_t size = f.tellg();
        f.seekg(0);
        std::vector<uint32_t> buf(size / 4);
        f.read(reinterpret_cast<char*>(buf.data()), size);
        return buf;
    }
};
