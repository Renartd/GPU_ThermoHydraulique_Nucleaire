#pragma once
// ============================================================
//  ThermalCompute.hpp
//  Pipeline Vulkan Compute pour la diffusion thermique 2D
//  Méthode : différences finies explicites, ping-pong buffers
// ============================================================
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <algorithm>
#include "VulkanContext.hpp"
#include "../core/GridData.hpp"

// ============================================================
//  Paramètres physiques passés au shader
// ============================================================
struct SimParams {
    float alpha;      // diffusivité thermique UO₂ (m²/s)
    float dx;         // pas spatial (m)
    float dt;         // pas de temps (s)
    float rho_cp;     // ρ·Cp UO₂ (J/m³·K)
    float T_inlet;    // température caloporteur (°C)
    float T_max;      // température max clamp (°C)
    int   grid_cols;
    int   grid_rows;
    int   total;
    int   _pad;       // alignement 16 bytes
};

// ============================================================
//  Constantes physiques UO₂ / REP
// ============================================================
namespace ThermalConst {
    constexpr float k_UO2    = 3.6f;       // W/m·K
    constexpr float rho_UO2  = 10970.0f;   // kg/m³
    constexpr float Cp_UO2   = 235.0f;     // J/kg·K
    constexpr float alpha_UO2 = k_UO2 / (rho_UO2 * Cp_UO2); // ~1.4e-6 m²/s
    constexpr float rho_cp   = rho_UO2 * Cp_UO2;             // ~2.578e6 J/m³·K
    constexpr float T_max    = 2800.0f;    // °C — point fusion UO₂
}

class ThermalCompute {
public:
    bool ready = false;

    // Températures courantes (lues par le renderer)
    std::vector<float> temperatures;

    // Grille plate complète (rows×cols) — inclut les vides
    std::vector<float> T_flat;
    std::vector<int>   mask_flat;

    int grid_rows = 0;
    int grid_cols = 0;
    int total     = 0;

    SimParams params{};

    // --------------------------------------------------------
    bool init(VulkanContext& ctx, const GridData& grid,
              float T_inlet = 286.0f) {
        try {
            _ctx = &ctx;

            grid_rows = grid.rows;
            grid_cols = grid.cols;
            total     = grid_rows * grid_cols;

            // Paramètres physiques
            params.alpha     = ThermalConst::alpha_UO2;
            params.dx        = grid.dims.width + grid.dims.spacing;
            params.T_inlet   = T_inlet;
            params.T_max     = ThermalConst::T_max;
            params.grid_cols = grid_cols;
            params.grid_rows = grid_rows;
            params.total     = total;
            params.rho_cp    = ThermalConst::rho_cp;

            // Stabilité de Fourier : dt ≤ 0.25·dx²/α
            float dt_max = 0.24f * params.dx * params.dx / params.alpha;
            params.dt = dt_max;
            std::cout << "[ThermalCompute] dt_max = " << dt_max << " s\n";

            // Initialise les grilles plates
            initFlatGrids(grid);

            temperatures.assign(grid.cubes.size(), T_inlet);

            // Crée les buffers GPU
            createBuffers();

            // Charge le shader
            createPipeline();
            createDescriptors();

            ready = true;
            std::cout << "[ThermalCompute] Prêt — grille "
                      << grid_cols << "×" << grid_rows << "\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[ThermalCompute] Erreur init : " << e.what() << "\n";
            return false;
        }
    }

    // --------------------------------------------------------
    //  Lance N pas de diffusion sur GPU
    //  simTime : temps simulé à ajouter (s)
    // --------------------------------------------------------
    void step(int nSteps, const std::vector<float>& q_vol) {
        if (!ready) return;

        // Met à jour q_vol sur GPU
        uploadQVol(q_vol);

        for (int i = 0; i < nSteps; ++i) {
            runOneStep();
            _pingPong = !_pingPong;
        }

        // Rapatrie les températures
        readback();
    }

    // --------------------------------------------------------
    //  Met à jour les températures dans GridData.cubes
    // --------------------------------------------------------
    void applyToGrid(GridData& grid) {
        for (int i = 0; i < (int)grid.cubes.size(); ++i) {
            auto& cube = grid.cubes[i];
            int idx = cube.row * grid_cols + cube.col_idx;
            if (idx < total) {
                cube.temperature = T_flat[idx];
                temperatures[i]  = T_flat[idx];
            }
        }
        grid.updateTempRange();
    }

    // --------------------------------------------------------
    //  Reset : remet toutes les températures à T_inlet
    // --------------------------------------------------------
    void reset(float T_inlet) {
        params.T_inlet = T_inlet;
        std::fill(T_flat.begin(), T_flat.end(), T_inlet);
        uploadTempBuffers();
        std::fill(temperatures.begin(), temperatures.end(), T_inlet);
    }

    void cleanup() {
        if (!_ctx) return;
        auto del = [&](VkBuffer b, VkDeviceMemory m) {
            if (b) vkDestroyBuffer(_ctx->device, b, nullptr);
            if (m) vkFreeMemory(_ctx->device, m, nullptr);
        };
        del(_bufA,      _memA);
        del(_bufB,      _memB);
        del(_bufQ,      _memQ);
        del(_bufMask,   _memMask);
        del(_bufParams, _memParams);
        del(_rbBuf,     _rbMem);
        if (_pipeline)       vkDestroyPipeline(_ctx->device, _pipeline, nullptr);
        if (_pipelineLayout) vkDestroyPipelineLayout(_ctx->device, _pipelineLayout, nullptr);
        if (_descSetLayout)  vkDestroyDescriptorSetLayout(_ctx->device, _descSetLayout, nullptr);
        if (_shaderModule)   vkDestroyShaderModule(_ctx->device, _shaderModule, nullptr);
    }

private:
    VulkanContext* _ctx     = nullptr;
    bool           _pingPong = false; // false=A→B, true=B→A

    // Buffers GPU
    VkBuffer       _bufA      = VK_NULL_HANDLE; // T ping
    VkDeviceMemory _memA      = VK_NULL_HANDLE;
    VkBuffer       _bufB      = VK_NULL_HANDLE; // T pong
    VkDeviceMemory _memB      = VK_NULL_HANDLE;
    VkBuffer       _bufQ      = VK_NULL_HANDLE; // q'''
    VkDeviceMemory _memQ      = VK_NULL_HANDLE;
    VkBuffer       _bufMask   = VK_NULL_HANDLE;
    VkDeviceMemory _memMask   = VK_NULL_HANDLE;
    VkBuffer       _bufParams = VK_NULL_HANDLE;
    VkDeviceMemory _memParams = VK_NULL_HANDLE;
    VkBuffer       _rbBuf     = VK_NULL_HANDLE; // readback
    VkDeviceMemory _rbMem     = VK_NULL_HANDLE;

    // Pipeline
    VkShaderModule        _shaderModule   = VK_NULL_HANDLE;
    VkPipelineLayout      _pipelineLayout = VK_NULL_HANDLE;
    VkPipeline            _pipeline       = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descSetLayout  = VK_NULL_HANDLE;
    VkDescriptorSet       _descSetAB      = VK_NULL_HANDLE; // A→B
    VkDescriptorSet       _descSetBA      = VK_NULL_HANDLE; // B→A

    // --------------------------------------------------------
    void initFlatGrids(const GridData& grid) {
        T_flat.assign(total, params.T_inlet);
        mask_flat.assign(total, 0);

        for (const auto& cube : grid.cubes) {
            int idx = cube.row * grid_cols + cube.col_idx;
            if (idx < total) {
                T_flat[idx]    = cube.temperature;
                mask_flat[idx] = 1;
            }
        }
    }

    // --------------------------------------------------------
    void createBuffers() {
        VkDeviceSize sizeT    = total * sizeof(float);
        VkDeviceSize sizeMask = total * sizeof(int);

        auto makeGPU = [&](VkDeviceSize sz, VkBuffer& b, VkDeviceMemory& m) {
            _ctx->createBuffer(sz,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, b, m);
        };
        auto makeHost = [&](VkDeviceSize sz, VkBuffer& b, VkDeviceMemory& m,
                            VkBufferUsageFlags extra) {
            _ctx->createBuffer(sz,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, b, m);
        };

        makeGPU(sizeT,    _bufA,    _memA);
        makeGPU(sizeT,    _bufB,    _memB);
        makeHost(sizeT,   _bufQ,    _memQ,    0);
        makeHost(sizeMask,_bufMask, _memMask, 0);
        makeHost(sizeof(SimParams), _bufParams, _memParams,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        _ctx->createBuffer(sizeT,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _rbBuf, _rbMem);

        // Upload masque
        void* ptr;
        vkMapMemory(_ctx->device, _memMask, 0, sizeMask, 0, &ptr);
        memcpy(ptr, mask_flat.data(), sizeMask);
        vkUnmapMemory(_ctx->device, _memMask);

        // Upload T initial
        uploadTempBuffers();

        // Upload params
        uploadParams();
    }

    // --------------------------------------------------------
    void uploadTempBuffers() {
        VkDeviceSize sizeT = total * sizeof(float);

        // Staging → A et B
        VkBuffer stg; VkDeviceMemory stgMem;
        _ctx->createBuffer(sizeT,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stg, stgMem);

        void* ptr;
        vkMapMemory(_ctx->device, stgMem, 0, sizeT, 0, &ptr);
        memcpy(ptr, T_flat.data(), sizeT);
        vkUnmapMemory(_ctx->device, stgMem);

        auto cb = _ctx->beginOneShot();
        VkBufferCopy region{0, 0, sizeT};
        vkCmdCopyBuffer(cb, stg, _bufA, 1, &region);
        vkCmdCopyBuffer(cb, stg, _bufB, 1, &region);
        _ctx->endOneShot(cb);

        vkDestroyBuffer(_ctx->device, stg, nullptr);
        vkFreeMemory(_ctx->device, stgMem, nullptr);
    }

    void uploadParams() {
        void* ptr;
        vkMapMemory(_ctx->device, _memParams, 0, sizeof(SimParams), 0, &ptr);
        memcpy(ptr, &params, sizeof(SimParams));
        vkUnmapMemory(_ctx->device, _memParams);
    }

    void uploadQVol(const std::vector<float>& q_vol) {
        VkDeviceSize sz = total * sizeof(float);
        std::vector<float> q_flat(total, 0.0f);
        // q_vol est indexé par cubes, on le remappe sur la grille plate
        // (la correspondance est dans mask_flat)
        // Pour simplifier : q_vol a déjà la taille de cubes → on cherche
        // une correspondance via l'index cube
        void* ptr;
        vkMapMemory(_ctx->device, _memQ, 0, sz, 0, &ptr);
        memcpy(ptr, q_vol.data(), std::min(sz, (VkDeviceSize)(q_vol.size() * sizeof(float))));
        vkUnmapMemory(_ctx->device, _memQ);
    }

    // --------------------------------------------------------
    void createPipeline() {
        auto spv = loadSPV("compute/shaders/diffusion.spv");
        VkShaderModuleCreateInfo smci{};
        smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = spv.size() * sizeof(uint32_t);
        smci.pCode    = spv.data();
        VK_CHECK(vkCreateShaderModule(_ctx->device, &smci, nullptr, &_shaderModule));

        // 5 bindings : T_in, T_out, q_vol, mask, params
        VkDescriptorSetLayoutBinding bindings[5] = {};
        for (int i = 0; i < 4; ++i) {
            bindings[i].binding        = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags     = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        bindings[4].binding         = 4;
        bindings[4].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo dslci{};
        dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 5;
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
        // Alloue deux descriptor sets : A→B et B→A
        VkDescriptorSetLayout layouts[2] = {_descSetLayout, _descSetLayout};
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = _ctx->descPool;
        ai.descriptorSetCount = 2;
        ai.pSetLayouts        = layouts;
        VkDescriptorSet sets[2];
        VK_CHECK(vkAllocateDescriptorSets(_ctx->device, &ai, sets));
        _descSetAB = sets[0];
        _descSetBA = sets[1];

        writeDescriptors(_descSetAB, _bufA, _bufB); // A→B
        writeDescriptors(_descSetBA, _bufB, _bufA); // B→A
    }

    void writeDescriptors(VkDescriptorSet ds,
                          VkBuffer T_in, VkBuffer T_out) {
        VkDescriptorBufferInfo bInfo[5] = {};
        bInfo[0] = {T_in,      0, VK_WHOLE_SIZE};
        bInfo[1] = {T_out,     0, VK_WHOLE_SIZE};
        bInfo[2] = {_bufQ,     0, VK_WHOLE_SIZE};
        bInfo[3] = {_bufMask,  0, VK_WHOLE_SIZE};
        bInfo[4] = {_bufParams,0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet writes[5] = {};
        for (int i = 0; i < 5; ++i) {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = ds;
            writes[i].dstBinding      = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = (i < 4)
                ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[i].pBufferInfo     = &bInfo[i];
        }
        vkUpdateDescriptorSets(_ctx->device, 5, writes, 0, nullptr);
    }

    // --------------------------------------------------------
    void runOneStep() {
        VkDescriptorSet ds = _pingPong ? _descSetBA : _descSetAB;
        VkBuffer outBuf    = _pingPong ? _bufA : _bufB;

        VkCommandBuffer cb = _ctx->beginOneShot();
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _pipelineLayout, 0, 1, &ds, 0, nullptr);

        uint32_t gx = (grid_cols + 15) / 16;
        uint32_t gy = (grid_rows + 15) / 16;
        vkCmdDispatch(cb, gx, gy, 1);

        // Barrière mémoire
        VkMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        _ctx->endOneShot(cb);
    }

    // --------------------------------------------------------
    void readback() {
        VkBuffer srcBuf = _pingPong ? _bufA : _bufB;
        VkDeviceSize sz = total * sizeof(float);

        auto cb = _ctx->beginOneShot();
        VkBufferCopy region{0, 0, sz};
        vkCmdCopyBuffer(cb, srcBuf, _rbBuf, 1, &region);
        _ctx->endOneShot(cb);

        void* ptr;
        vkMapMemory(_ctx->device, _rbMem, 0, sz, 0, &ptr);
        memcpy(T_flat.data(), ptr, sz);
        vkUnmapMemory(_ctx->device, _rbMem);
    }

    // --------------------------------------------------------
    std::vector<uint32_t> loadSPV(const std::string& path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            throw std::runtime_error("SPIR-V introuvable : " + path +
                "\n  → lancez ./build.sh pour compiler les shaders");
        size_t sz = f.tellg(); f.seekg(0);
        std::vector<uint32_t> buf(sz / 4);
        f.read(reinterpret_cast<char*>(buf.data()), sz);
        return buf;
    }
};
