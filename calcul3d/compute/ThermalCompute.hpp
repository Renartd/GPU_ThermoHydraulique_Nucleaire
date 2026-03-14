#pragma once
// ============================================================
//  ThermalCompute.hpp
//  Pipeline Vulkan Compute — diffusion thermique 3D
//  Grille : cols × rows × slices  (ping-pong buffers)
//  Choix slices : 4 / 8 / 16 / 32
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

// VK_CHECK local (défini aussi dans NeutronCompute — guard contre double-def)
#ifndef VK_CHECK
#define VK_CHECK(call) do { \
    VkResult _r=(call); \
    if(_r!=VK_SUCCESS) \
        throw std::runtime_error(std::string("[VK] ")+#call+" = "+std::to_string(_r)); \
} while(0)
#endif

// ============================================================
//  SimParams — doit correspondre exactement au uniform du shader
// ============================================================
struct SimParams {
    float alpha;        // diffusivité thermique UO₂ (m²/s)
    float dx;           // pas spatial X/Z (m)
    float dt;           // pas de temps (s)
    float rho_cp;       // ρ·Cp UO₂ (J/m³·K)
    float T_inlet;      // température caloporteur entrée (°C)
    float T_max;        // température max clamp (°C)
    int   grid_cols;
    int   grid_rows;
    int   grid_slices;  // tranches verticales
    float dy;           // pas spatial Y = H_coeur/slices (m)
    int   total;        // cols×rows×slices
    int   _pad;
};

// ============================================================
//  Constantes physiques UO₂
// ============================================================
namespace ThermalConst {
    constexpr float k_UO2     = 3.6f;
    constexpr float rho_UO2   = 10970.0f;
    constexpr float Cp_UO2    = 235.0f;
    constexpr float alpha_UO2 = k_UO2 / (rho_UO2 * Cp_UO2);
    constexpr float rho_cp    = rho_UO2 * Cp_UO2;
    constexpr float T_max     = 2800.0f;
}

// ============================================================
class ThermalCompute {
public:
    bool ready = false;

    // Températures 3D linéarisées : slice-major
    // index = slice * total2d + row * grid_cols + col
    std::vector<float> T_flat3d;

    // Températures 2D (moyenne par assemblage, pour compatibilité)
    std::vector<float> T_flat;
    std::vector<int>   mask_flat;

    int grid_rows   = 0;
    int grid_cols   = 0;
    int grid_slices = 8;
    int total       = 0;   // cols×rows×slices
    int total2d     = 0;   // cols×rows

    SimParams params{};

    // --------------------------------------------------------
    bool init(VulkanContext& ctx, const GridData& grid,
              float T_inlet = 286.0f, int nSlices = 8) {
        try {
            _ctx = &ctx;
            grid_rows   = grid.rows;
            grid_cols   = grid.cols;
            grid_slices = nSlices;
            total2d     = grid_rows * grid_cols;
            total       = total2d * grid_slices;

            params.alpha       = ThermalConst::alpha_UO2;
            params.dx          = grid.dims.width + grid.dims.spacing;
            params.dy          = grid.dims.height / (float)grid_slices;
            params.T_inlet     = T_inlet;
            params.T_max       = ThermalConst::T_max;
            params.grid_cols   = grid_cols;
            params.grid_rows   = grid_rows;
            params.grid_slices = grid_slices;
            params.total       = total;
            params.rho_cp      = ThermalConst::rho_cp;

            // Stabilité Fourier 3D
            float inv_dx2 = 1.0f / (params.dx * params.dx);
            float inv_dy2 = 1.0f / (params.dy * params.dy);
            params.dt = 0.9f / (params.alpha * (4.0f * inv_dx2 + inv_dy2));
            std::cout << "[ThermalCompute3D] slices=" << grid_slices
                      << "  dy=" << params.dy << " m  dt=" << params.dt << " s\n";

            initFlatGrids(grid);
            createBuffers();
            createPipeline();
            createDescriptors();

            ready = true;
            std::cout << "[ThermalCompute3D] Prêt — "
                      << grid_cols << "×" << grid_rows << "×" << grid_slices
                      << " = " << total << " nœuds\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[ThermalCompute3D] Erreur : " << e.what() << "\n";
            return false;
        }
    }

    // ---- Changement de résolution à chaud ----
    bool reinit(VulkanContext& ctx, const GridData& grid,
                float T_inlet, int nSlices) {
        cleanup();
        ready = false;
        _pingPong = false;
        return init(ctx, grid, T_inlet, nSlices);
    }

    // --------------------------------------------------------
    void step(int nSteps, const std::vector<float>& q_vol_2d) {
        if (!ready) return;
        uploadQVol(q_vol_2d);
        for (int i = 0; i < nSteps; ++i) {
            runOneStep();
            _pingPong = !_pingPong;
        }
        readback();
        computeAverage();
    }

    // --------------------------------------------------------
    //  Copie T_axial (profil vertical) + température moyenne dans chaque cube
    // --------------------------------------------------------
    void applyToGrid(GridData& grid) {
        for (auto& cube : grid.cubes) {
            int i2d = cube.row * grid_cols + cube.col_idx;
            if (i2d >= total2d) continue;

            cube.temperature = T_flat[i2d];

            cube.T_axial.resize(grid_slices);
            for (int s = 0; s < grid_slices; ++s)
                cube.T_axial[s] = T_flat3d[s * total2d + i2d];
        }
        grid.updateTempRange();
    }

    // --------------------------------------------------------
    void reset(float T_inlet) {
        params.T_inlet = T_inlet;
        std::fill(T_flat3d.begin(), T_flat3d.end(), T_inlet);
        std::fill(T_flat.begin(),   T_flat.end(),   T_inlet);
        uploadTempBuffers();
        uploadParams();
    }

    // --------------------------------------------------------
    void cleanup() {
        if (!_ctx) return;
        auto del = [&](VkBuffer b, VkDeviceMemory m) {
            if (b) vkDestroyBuffer(_ctx->device, b, nullptr);
            if (m) vkFreeMemory(_ctx->device, m, nullptr);
        };
        del(_bufA, _memA); del(_bufB, _memB);
        del(_bufQ, _memQ); del(_bufMask, _memMask);
        del(_bufParams, _memParams); del(_rbBuf, _rbMem);
        if (_pipeline)       vkDestroyPipeline(_ctx->device, _pipeline, nullptr);
        if (_pipelineLayout) vkDestroyPipelineLayout(_ctx->device, _pipelineLayout, nullptr);
        if (_descSetLayout)  vkDestroyDescriptorSetLayout(_ctx->device, _descSetLayout, nullptr);
        if (_shaderModule)   vkDestroyShaderModule(_ctx->device, _shaderModule, nullptr);
        _pipeline=VK_NULL_HANDLE; _pipelineLayout=VK_NULL_HANDLE;
        _descSetLayout=VK_NULL_HANDLE; _shaderModule=VK_NULL_HANDLE;
        _bufA=_bufB=_bufQ=_bufMask=_bufParams=_rbBuf=VK_NULL_HANDLE;
        _memA=_memB=_memQ=_memMask=_memParams=_rbMem=VK_NULL_HANDLE;
    }

private:
    VulkanContext* _ctx      = nullptr;
    bool           _pingPong = false;

    VkBuffer       _bufA=VK_NULL_HANDLE, _bufB=VK_NULL_HANDLE;
    VkDeviceMemory _memA=VK_NULL_HANDLE, _memB=VK_NULL_HANDLE;
    VkBuffer       _bufQ=VK_NULL_HANDLE, _bufMask=VK_NULL_HANDLE, _bufParams=VK_NULL_HANDLE;
    VkDeviceMemory _memQ=VK_NULL_HANDLE, _memMask=VK_NULL_HANDLE, _memParams=VK_NULL_HANDLE;
    VkBuffer       _rbBuf=VK_NULL_HANDLE;
    VkDeviceMemory _rbMem=VK_NULL_HANDLE;

    VkShaderModule        _shaderModule   = VK_NULL_HANDLE;
    VkPipelineLayout      _pipelineLayout = VK_NULL_HANDLE;
    VkPipeline            _pipeline       = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descSetLayout  = VK_NULL_HANDLE;
    VkDescriptorSet       _descSetAB      = VK_NULL_HANDLE;
    VkDescriptorSet       _descSetBA      = VK_NULL_HANDLE;

    // --------------------------------------------------------
    void initFlatGrids(const GridData& grid) {
        T_flat3d.assign(total,    params.T_inlet);
        T_flat.assign(total2d,    params.T_inlet);
        mask_flat.assign(total2d, 0);

        for (const auto& cube : grid.cubes) {
            int i2d = cube.row * grid_cols + cube.col_idx;
            if (i2d < total2d) {
                mask_flat[i2d] = 1;
                for (int s = 0; s < grid_slices; ++s)
                    T_flat3d[s * total2d + i2d] = cube.temperature;
                T_flat[i2d] = cube.temperature;
            }
        }
    }

    void computeAverage() {
        for (int i2d = 0; i2d < total2d; ++i2d) {
            if (!mask_flat[i2d]) { T_flat[i2d] = params.T_inlet; continue; }
            float sum = 0.0f;
            for (int s = 0; s < grid_slices; ++s)
                sum += T_flat3d[s * total2d + i2d];
            T_flat[i2d] = sum / (float)grid_slices;
        }
    }

    // --------------------------------------------------------
    void createBuffers() {
        VkDeviceSize sz3d   = total   * sizeof(float);
        VkDeviceSize sz2d   = total2d * sizeof(float);
        VkDeviceSize szMask = total2d * sizeof(int);

        auto makeGPU = [&](VkDeviceSize sz, VkBuffer& b, VkDeviceMemory& m) {
            _ctx->createBuffer(sz,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT   |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, b, m);
        };
        auto makeHost = [&](VkDeviceSize sz, VkBuffer& b, VkDeviceMemory& m,
                            VkBufferUsageFlags extra = 0) {
            _ctx->createBuffer(sz,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, b, m);
        };

        makeGPU(sz3d,   _bufA,      _memA);
        makeGPU(sz3d,   _bufB,      _memB);
        makeHost(sz2d,  _bufQ,      _memQ);
        makeHost(szMask,_bufMask,   _memMask);
        makeHost(sizeof(SimParams), _bufParams, _memParams,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        _ctx->createBuffer(sz3d,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _rbBuf, _rbMem);

        void* ptr;
        vkMapMemory(_ctx->device, _memMask, 0, szMask, 0, &ptr);
        memcpy(ptr, mask_flat.data(), szMask);
        vkUnmapMemory(_ctx->device, _memMask);

        uploadTempBuffers();
        uploadParams();
    }

    void uploadTempBuffers() {
        VkDeviceSize sz = total * sizeof(float);
        VkBuffer stg; VkDeviceMemory stgMem;
        _ctx->createBuffer(sz,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stg, stgMem);

        void* ptr;
        vkMapMemory(_ctx->device, stgMem, 0, sz, 0, &ptr);
        memcpy(ptr, T_flat3d.data(), sz);
        vkUnmapMemory(_ctx->device, stgMem);

        auto cb = _ctx->beginOneShot();
        VkBufferCopy region{0, 0, sz};
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

    void uploadQVol(const std::vector<float>& q_vol_2d) {
        VkDeviceSize sz = total2d * sizeof(float);
        void* ptr;
        vkMapMemory(_ctx->device, _memQ, 0, sz, 0, &ptr);
        size_t cpSz = std::min((size_t)total2d, q_vol_2d.size()) * sizeof(float);
        memcpy(ptr, q_vol_2d.data(), cpSz);
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

        VkDescriptorSetLayoutBinding bindings[5] = {};
        for (int i = 0; i < 4; ++i) {
            bindings[i].binding         = i;
            bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        bindings[4] = {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

        VkDescriptorSetLayoutCreateInfo dslci{};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 5; dslci.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(_ctx->device, &dslci, nullptr, &_descSetLayout));

        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1; plci.pSetLayouts = &_descSetLayout;
        VK_CHECK(vkCreatePipelineLayout(_ctx->device, &plci, nullptr, &_pipelineLayout));

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = _shaderModule; stage.pName = "main";

        VkComputePipelineCreateInfo cpci{};
        cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpci.stage = stage; cpci.layout = _pipelineLayout;
        VK_CHECK(vkCreateComputePipelines(_ctx->device, VK_NULL_HANDLE,
                                          1, &cpci, nullptr, &_pipeline));
    }

    void createDescriptors() {
        VkDescriptorSetLayout layouts[2] = {_descSetLayout, _descSetLayout};
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = _ctx->descriptorPool;
        ai.descriptorSetCount = 2; ai.pSetLayouts = layouts;
        VkDescriptorSet sets[2];
        VK_CHECK(vkAllocateDescriptorSets(_ctx->device, &ai, sets));
        _descSetAB = sets[0]; _descSetBA = sets[1];
        writeDescriptors(_descSetAB, _bufA, _bufB);
        writeDescriptors(_descSetBA, _bufB, _bufA);
    }

    void writeDescriptors(VkDescriptorSet ds, VkBuffer T_in, VkBuffer T_out) {
        VkDescriptorBufferInfo bi[5] = {
            {T_in,       0, VK_WHOLE_SIZE},
            {T_out,      0, VK_WHOLE_SIZE},
            {_bufQ,      0, VK_WHOLE_SIZE},
            {_bufMask,   0, VK_WHOLE_SIZE},
            {_bufParams, 0, VK_WHOLE_SIZE},
        };
        VkWriteDescriptorSet writes[5] = {};
        for (int i = 0; i < 5; ++i) {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = ds;
            writes[i].dstBinding      = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = (i < 4) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                                 : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[i].pBufferInfo     = &bi[i];
        }
        vkUpdateDescriptorSets(_ctx->device, 5, writes, 0, nullptr);
    }

    // --------------------------------------------------------
    void runOneStep() {
        VkDescriptorSet ds = _pingPong ? _descSetBA : _descSetAB;

        VkCommandBuffer cb = _ctx->beginOneShot();
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _pipelineLayout, 0, 1, &ds, 0, nullptr);
        // Dispatch 3D — groupes de 8×8×4
        uint32_t gx = (grid_cols   + 7) / 8;
        uint32_t gy = (grid_rows   + 7) / 8;
        uint32_t gz = (grid_slices + 3) / 4;
        vkCmdDispatch(cb, gx, gy, gz);

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

    void readback() {
        VkBuffer srcBuf = _pingPong ? _bufA : _bufB;
        VkDeviceSize sz = total * sizeof(float);
        auto cb = _ctx->beginOneShot();
        VkBufferCopy region{0, 0, sz};
        vkCmdCopyBuffer(cb, srcBuf, _rbBuf, 1, &region);
        _ctx->endOneShot(cb);

        void* ptr;
        vkMapMemory(_ctx->device, _rbMem, 0, sz, 0, &ptr);
        memcpy(T_flat3d.data(), ptr, sz);
        vkUnmapMemory(_ctx->device, _rbMem);
    }

    std::vector<uint32_t> loadSPV(const std::string& path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            throw std::runtime_error("SPIR-V introuvable : " + path);
        size_t sz = f.tellg(); f.seekg(0);
        std::vector<uint32_t> buf(sz / 4);
        f.read(reinterpret_cast<char*>(buf.data()), sz);
        return buf;
    }
};