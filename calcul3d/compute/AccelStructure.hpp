#pragma once
// ============================================================
//  AccelStructure.hpp
//  Construit la BVH (BLAS + TLAS) depuis la grille d'assemblages
//
//  BLAS (Bottom Level AS) = géométrie d'un cube unitaire
//  TLAS (Top Level AS)    = instances = un cube par assemblage
// ============================================================

#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>
#include <stdexcept>
#include "VulkanContext.hpp"
#include "../core/GridData.hpp"

// Fonctions RT à charger dynamiquement
#define LOAD_VK_FUNC(ctx, name) \
    auto name = (PFN_##name)vkGetDeviceProcAddr(ctx.device, #name); \
    if (!name) throw std::runtime_error("Impossible de charger " #name);

struct AccelStructure {

    // BLAS — géométrie cube
    VkAccelerationStructureKHR blas       = VK_NULL_HANDLE;
    VkBuffer                   blasBuf    = VK_NULL_HANDLE;
    VkDeviceMemory             blasMem    = VK_NULL_HANDLE;

    // TLAS — instances (une par assemblage)
    VkAccelerationStructureKHR tlas       = VK_NULL_HANDLE;
    VkBuffer                   tlasBuf    = VK_NULL_HANDLE;
    VkDeviceMemory             tlasMem    = VK_NULL_HANDLE;

    // Buffer de vertices du cube
    VkBuffer                   vertBuf    = VK_NULL_HANDLE;
    VkDeviceMemory             vertMem    = VK_NULL_HANDLE;
    VkBuffer                   idxBuf     = VK_NULL_HANDLE;
    VkDeviceMemory             idxMem     = VK_NULL_HANDLE;

    // Buffer instances TLAS
    VkBuffer                   instBuf    = VK_NULL_HANDLE;
    VkDeviceMemory             instMem    = VK_NULL_HANDLE;

    // Adresse device du TLAS (utilisée dans le descriptor)
    VkDeviceAddress            tlasAddress = 0;

    // --------------------------------------------------------
    //  Construction complète BLAS + TLAS
    // --------------------------------------------------------
    void build(VulkanContext& ctx, const GridData& grid) {
        uploadCubeGeometry(ctx);
        buildBLAS(ctx);
        buildTLAS(ctx, grid);
        std::cout << "[AccelStructure] BVH construite : "
                  << grid.cubes.size() << " instances\n";
    }

    void cleanup(VulkanContext& ctx) {
        LOAD_VK_FUNC(ctx, vkDestroyAccelerationStructureKHR);
        if (tlas)    vkDestroyAccelerationStructureKHR(ctx.device, tlas, nullptr);
        if (blas)    vkDestroyAccelerationStructureKHR(ctx.device, blas, nullptr);
        auto destroy = [&](VkBuffer b, VkDeviceMemory m) {
            if (b) vkDestroyBuffer(ctx.device, b, nullptr);
            if (m) vkFreeMemory(ctx.device, m, nullptr);
        };
        destroy(tlasBuf, tlasMem);
        destroy(blasBuf, blasMem);
        destroy(instBuf, instMem);
        destroy(vertBuf, vertMem);
        destroy(idxBuf,  idxMem);
    }

private:

    // --------------------------------------------------------
    //  Géométrie cube unitaire [-0.5, 0.5]
    // --------------------------------------------------------
    void uploadCubeGeometry(VulkanContext& ctx) {
        // 8 sommets du cube
        const float verts[] = {
            -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
             0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
             0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        };
        // 12 triangles = 6 faces
        const uint32_t idx[] = {
            0,1,2, 2,3,0,   4,5,6, 6,7,4,
            0,4,7, 7,3,0,   1,5,6, 6,2,1,
            3,2,6, 6,7,3,   0,1,5, 5,4,0,
        };

        VkBufferUsageFlags usage =
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        uploadBuffer(ctx, verts, sizeof(verts), usage, vertBuf, vertMem);
        uploadBuffer(ctx, idx,   sizeof(idx),   usage, idxBuf,  idxMem);
    }

    // --------------------------------------------------------
    //  Build BLAS depuis le cube
    // --------------------------------------------------------
    void buildBLAS(VulkanContext& ctx) {
        LOAD_VK_FUNC(ctx, vkGetAccelerationStructureBuildSizesKHR);
        LOAD_VK_FUNC(ctx, vkCreateAccelerationStructureKHR);
        LOAD_VK_FUNC(ctx, vkCmdBuildAccelerationStructuresKHR);
        LOAD_VK_FUNC(ctx, vkGetAccelerationStructureDeviceAddressKHR);

        VkAccelerationStructureGeometryTrianglesDataKHR triData{};
        triData.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triData.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
        triData.vertexData.deviceAddress = getBufferAddress(ctx, vertBuf);
        triData.vertexStride  = sizeof(float) * 3;
        triData.maxVertex     = 7;
        triData.indexType     = VK_INDEX_TYPE_UINT32;
        triData.indexData.deviceAddress = getBufferAddress(ctx, idxBuf);

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geom.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geom.geometry.triangles = triData;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &geom;

        uint32_t primCount = 12; // 12 triangles
        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(ctx.device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizes);

        createAS(ctx, sizes.accelerationStructureSize,
                 VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                 blas, blasBuf, blasMem);

        buildInfo.dstAccelerationStructure = blas;

        // Scratch buffer
        VkBuffer scratchBuf; VkDeviceMemory scratchMem;
        ctx.createBuffer(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            scratchBuf, scratchMem);
        buildInfo.scratchData.deviceAddress = getBufferAddress(ctx, scratchBuf);

        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = 12;
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

        auto cb = ctx.beginOneShot();
        vkCmdBuildAccelerationStructuresKHR(cb, 1, &buildInfo, &pRange);
        ctx.endOneShot(cb);

        vkDestroyBuffer(ctx.device, scratchBuf, nullptr);
        vkFreeMemory(ctx.device, scratchMem, nullptr);
    }

    // --------------------------------------------------------
    //  Build TLAS : une instance par assemblage
    // --------------------------------------------------------
    void buildTLAS(VulkanContext& ctx, const GridData& grid) {
        LOAD_VK_FUNC(ctx, vkGetAccelerationStructureDeviceAddressKHR);
        LOAD_VK_FUNC(ctx, vkGetAccelerationStructureBuildSizesKHR);
        LOAD_VK_FUNC(ctx, vkCreateAccelerationStructureKHR);
        LOAD_VK_FUNC(ctx, vkCmdBuildAccelerationStructuresKHR);

        // Récupère l'adresse de la BLAS
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = blas;
        VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(ctx.device, &addrInfo);

        // Crée une instance VkAccelerationStructureInstanceKHR par cube
        std::vector<VkAccelerationStructureInstanceKHR> instances;
        instances.reserve(grid.cubes.size());

        for (const auto& cube : grid.cubes) {
            VkAccelerationStructureInstanceKHR inst{};
            // Matrice de transformation 3x4 (row-major)
            // Translation vers la position du cube
            memset(&inst.transform, 0, sizeof(inst.transform));
            inst.transform.matrix[0][0] = 1.0f;
            inst.transform.matrix[1][1] = 1.0f;
            inst.transform.matrix[2][2] = 1.0f;
            inst.transform.matrix[0][3] = cube.pos.x;
            inst.transform.matrix[1][3] = 0.0f;       // y centré
            inst.transform.matrix[2][3] = cube.pos.z;

            inst.instanceCustomIndex                    = 0;
            inst.mask                                   = 0xFF;
            inst.instanceShaderBindingTableRecordOffset = 0;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference         = blasAddr;
            instances.push_back(inst);
        }

        uploadBuffer(ctx,
            instances.data(),
            instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            instBuf, instMem);

        VkAccelerationStructureGeometryInstancesDataKHR instData{};
        instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instData.data.deviceAddress = getBufferAddress(ctx, instBuf);

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geom.geometry.instances = instData;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &geom;

        uint32_t instCount = (uint32_t)instances.size();
        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(ctx.device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &instCount, &sizes);

        createAS(ctx, sizes.accelerationStructureSize,
                 VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                 tlas, tlasBuf, tlasMem);

        buildInfo.dstAccelerationStructure = tlas;

        VkBuffer scratchBuf; VkDeviceMemory scratchMem;
        ctx.createBuffer(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            scratchBuf, scratchMem);
        buildInfo.scratchData.deviceAddress = getBufferAddress(ctx, scratchBuf);

        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = instCount;
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

        auto cb = ctx.beginOneShot();
        vkCmdBuildAccelerationStructuresKHR(cb, 1, &buildInfo, &pRange);
        ctx.endOneShot(cb);

        vkDestroyBuffer(ctx.device, scratchBuf, nullptr);
        vkFreeMemory(ctx.device, scratchMem, nullptr);

        // Sauvegarde l'adresse TLAS pour le descriptor
        VkAccelerationStructureDeviceAddressInfoKHR tlasAddrInfo{};
        tlasAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        tlasAddrInfo.accelerationStructure = tlas;
        tlasAddress = vkGetAccelerationStructureDeviceAddressKHR(ctx.device, &tlasAddrInfo);
    }

    // --------------------------------------------------------
    //  Helpers
    // --------------------------------------------------------
    void createAS(VulkanContext& ctx,
                  VkDeviceSize size,
                  VkAccelerationStructureTypeKHR type,
                  VkAccelerationStructureKHR& as,
                  VkBuffer& buf,
                  VkDeviceMemory& mem) {
        LOAD_VK_FUNC(ctx, vkCreateAccelerationStructureKHR);

        ctx.createBuffer(size,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            buf, mem);

        VkAccelerationStructureCreateInfoKHR ci{};
        ci.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        ci.buffer = buf;
        ci.size   = size;
        ci.type   = type;
        VK_CHECK(vkCreateAccelerationStructureKHR(ctx.device, &ci, nullptr, &as));
    }

    VkDeviceAddress getBufferAddress(VulkanContext& ctx, VkBuffer buf) {
        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buf;
        return vkGetBufferDeviceAddress(ctx.device, &info);
    }

    void uploadBuffer(VulkanContext& ctx,
                      const void* data, VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkBuffer& buf, VkDeviceMemory& mem) {
        ctx.createBuffer(size, usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buf, mem);
        void* ptr;
        vkMapMemory(ctx.device, mem, 0, size, 0, &ptr);
        memcpy(ptr, data, size);
        vkUnmapMemory(ctx.device, mem);
    }
};
