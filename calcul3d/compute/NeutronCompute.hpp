#pragma once
// ============================================================
//  NeutronCompute.hpp  —  Pipeline Vulkan diffusion 2 groupes
//  FDM explicite + iteration de puissance CPU pour k_eff
// ============================================================
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include "VulkanContext.hpp"
#include "../core/GridData.hpp"
#include "../physics/NeutronCrossSection.hpp"

struct NeutronParams {
    float dt; float dx; float k_eff; float phi0;
    int grid_cols; int grid_rows; int total2d;
    float v1; float v2; float T_inlet; int _pad[2];
};

class NeutronCompute {
public:
    bool  ready = false;
    float k_eff = 1.0f;
    std::vector<float> phi1_flat, phi2_flat, phi_total, xs_flat;
    int grid_cols=0, grid_rows=0, total2d=0;
    NeutronParams params{};

    struct ModNode { int i2d; float rho_rel; };
    struct CtrlRod { int row, col; float insertFraction; };
    std::vector<ModNode> _moderatorNodes;
    std::vector<int>     _reflectorNodes;
    std::vector<CtrlRod> _controlRods;

    void setControlRod(int row, int col, float ins) {
        for (auto& c:_controlRods)
            if (c.row==row&&c.col==col) { c.insertFraction=ins; return; }
        _controlRods.push_back({row,col,ins});
    }

    bool init(VulkanContext& ctx, const GridData& grid,
              ReactorType rt, float enrichment, float T0=300.0f)
    {
        try {
            _ctx=&ctx; _rt=rt; _epsilon=enrichment;
            grid_cols=grid.cols; grid_rows=grid.rows; total2d=grid_cols*grid_rows;
            float dx_cm=(grid.dims.width+grid.dims.spacing)*100.0f;
            params={5e-5f,dx_cm,1.0f,1.0f,grid_cols,grid_rows,total2d,1.0f,0.1f,286.0f,{0,0}};

            phi1_flat.assign(total2d,0.0f); phi2_flat.assign(total2d,0.0f);
            phi_total.assign(total2d,0.0f); xs_flat.assign(total2d*9,0.0f);
            _zone_flat.assign(total2d,0);

            float R=sqrtf(grid.offsetX*grid.offsetX+grid.offsetZ*grid.offsetZ);
            if (R<0.1f) R=1.0f;
            for (const auto& c:grid.cubes) {
                int i=c.row*grid_cols+c.col_idx;
                float r=sqrtf(c.pos.x*c.pos.x+c.pos.z*c.pos.z);
                float v=cosf(fminf((float)M_PI*r/(2.0f*R*1.05f),(float)M_PI*0.499f));
                phi1_flat[i]=v*0.1f; phi2_flat[i]=v*0.9f; _zone_flat[i]=1;
            }

            for (const auto& zn:grid.zoneNodes) {
                if (zn.row<0||zn.row>=grid_rows||zn.col<0||zn.col>=grid_cols) continue;
                int i=zn.row*grid_cols+zn.col;
                if (zn.zone==NodeZone::MODERATOR)   { _moderatorNodes.push_back({i,zn.param}); _zone_flat[i]=2; }
                if (zn.zone==NodeZone::REFLECTOR)   { _reflectorNodes.push_back(i);             _zone_flat[i]=3; }
                if (zn.zone==NodeZone::CONTROL_ROD) { _controlRods.push_back({zn.row,zn.col,zn.param}); _zone_flat[i]=4; }
            }

            rebuildXS(grid,T0);
            createBuffers(); createPipeline(); createDescriptors();

            float dt_max=dx_cm*dx_cm/(2.0f*2.5f*params.v1);
            if (params.dt>dt_max) { params.dt=dt_max*0.9f; uploadParams(); }

            ready=true;
            std::cout<<"[NeutronCompute] OK — "<<NeutronCrossSection::reactorName(rt)
                     <<"  eps="<<enrichment*100.0f<<"% dx="<<dx_cm<<"cm\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr<<"[NeutronCompute] "<<e.what()<<"\n"; return false;
        }
    }

    void rebuildXS(const GridData& grid, float T0=300.0f, float rho_mod=1.0f) {
        for (int i=0;i<total2d;++i) writeXS(i,XS2G{});
        for (const auto& c:grid.cubes) {
            int i=c.row*grid_cols+c.col_idx; if(i<0||i>=total2d) continue;
            float T=(c.temperature>100.0f)?c.temperature:T0;
            writeXS(i,NeutronCrossSection::fuel(_rt,_epsilon,T,rho_mod)); _zone_flat[i]=1;
        }
        for (const auto& mn:_moderatorNodes)
            writeXS(mn.i2d,NeutronCrossSection::moderator(_rt,mn.rho_rel));
        for (int i:_reflectorNodes)
            writeXS(i,NeutronCrossSection::reflector(_rt));
        for (const auto& cr:_controlRods) {
            int i=cr.row*grid_cols+cr.col;
            if (i>=0&&i<total2d) writeXS(i,NeutronCrossSection::controlRod(cr.insertFraction));
        }
        if (ready) { uploadXS(); uploadZones(); }
    }

    void step(int nDiff, int nPower=3) {
        if (!ready) return;
        float fp=computeF();
        for (int pi=0;pi<nPower;++pi) {
            for (int i=0;i<nDiff;++i) { runOneStep(); _pingPong=!_pingPong; }
            readback();
            float fn=computeF();
            if (fp>1e-15f) k_eff*=(fn/fp);
            k_eff=fmaxf(0.001f,fminf(k_eff,10.0f));
            fp=(fn>1e-15f)?fn:fp;
            params.k_eff=k_eff; uploadParams();
        }
        float mx=1e-15f;
        for (int i=0;i<total2d;++i) mx=fmaxf(mx,phi1_flat[i]+phi2_flat[i]);
        for (int i=0;i<total2d;++i) phi_total[i]=(phi1_flat[i]+phi2_flat[i])/mx;
    }

    void applyToGrid(GridData& grid) {
        for (auto& c:grid.cubes) {
            int i=c.row*grid_cols+c.col_idx;
            if (i>=0&&i<total2d) c.flux=phi_total[i];
        }
    }

    void cleanup() {
        if (!_ctx) return;

        auto del=[&](VkBuffer b,VkDeviceMemory m){
            if(b) vkDestroyBuffer(_ctx->device,b,nullptr);
            if(m) vkFreeMemory(_ctx->device,m,nullptr);
        };

        del(_bufPhi1A,_memPhi1A); del(_bufPhi1B,_memPhi1B);
        del(_bufPhi2A,_memPhi2A); del(_bufPhi2B,_memPhi2B);
        del(_bufXS,_memXS); del(_bufZone,_memZone);
        del(_bufParams,_memParams); del(_rbBuf,_rbMem);

        if (_pipeline)       vkDestroyPipeline(_ctx->device,_pipeline,nullptr);
        if (_pipelineLayout) vkDestroyPipelineLayout(_ctx->device,_pipelineLayout,nullptr);
        if (_descSetLayout)  vkDestroyDescriptorSetLayout(_ctx->device,_descSetLayout,nullptr);
        if (_shaderModule)   vkDestroyShaderModule(_ctx->device,_shaderModule,nullptr);

        // --- Correction : remise à zéro explicite ---
        _pipeline       = VK_NULL_HANDLE;
        _pipelineLayout = VK_NULL_HANDLE;
        _descSetLayout  = VK_NULL_HANDLE;
        _shaderModule   = VK_NULL_HANDLE;
        _descSetAB      = VK_NULL_HANDLE;
        _descSetBA      = VK_NULL_HANDLE;

        _bufPhi1A=_bufPhi1B=_bufPhi2A=_bufPhi2B=VK_NULL_HANDLE;
        _bufXS=_bufZone=_bufParams=_rbBuf=VK_NULL_HANDLE;
        _memPhi1A=_memPhi1B=_memPhi2A=_memPhi2B=VK_NULL_HANDLE;
        _memXS=_memZone=_memParams=_rbMem=VK_NULL_HANDLE;

        _moderatorNodes.clear(); _reflectorNodes.clear(); _controlRods.clear();
        ready=false; k_eff=1.0f; _pingPong=false;
    }

private:
    VulkanContext* _ctx=nullptr;
    bool _pingPong=false;
    ReactorType _rt=ReactorType::REP;
    float _epsilon=0.035f;
    std::vector<int> _zone_flat;

    VkBuffer _bufPhi1A=VK_NULL_HANDLE,_bufPhi1B=VK_NULL_HANDLE;
    VkDeviceMemory _memPhi1A=VK_NULL_HANDLE,_memPhi1B=VK_NULL_HANDLE;
    VkBuffer _bufPhi2A=VK_NULL_HANDLE,_bufPhi2B=VK_NULL_HANDLE;
    VkDeviceMemory _memPhi2A=VK_NULL_HANDLE,_memPhi2B=VK_NULL_HANDLE;
    VkBuffer _bufXS=VK_NULL_HANDLE,_bufZone=VK_NULL_HANDLE;
    VkDeviceMemory _memXS=VK_NULL_HANDLE,_memZone=VK_NULL_HANDLE;
    VkBuffer _bufParams=VK_NULL_HANDLE,_rbBuf=VK_NULL_HANDLE;
    VkDeviceMemory _memParams=VK_NULL_HANDLE,_rbMem=VK_NULL_HANDLE;
    VkShaderModule _shaderModule=VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout=VK_NULL_HANDLE;
    VkPipeline _pipeline=VK_NULL_HANDLE;
    VkDescriptorSetLayout _descSetLayout=VK_NULL_HANDLE;
    VkDescriptorSet _descSetAB=VK_NULL_HANDLE,_descSetBA=VK_NULL_HANDLE;

    void writeXS(int i,const XS2G& xs){
        int b=i*9;
        xs_flat[b+0]=xs.D[0];xs_flat[b+1]=xs.D[1];
        xs_flat[b+2]=xs.SigR[0];xs_flat[b+3]=xs.SigR[1];
        xs_flat[b+4]=xs.SigS12;
        xs_flat[b+5]=xs.nuSigF[0];xs_flat[b+6]=xs.nuSigF[1];
        xs_flat[b+7]=xs.chi[0];xs_flat[b+8]=xs.chi[1];
    }

    float computeF(){
        float F=0.0f;
        for(int i=0;i<total2d;++i)
            F+=xs_flat[i*9+5]*phi1_flat[i]+xs_flat[i*9+6]*phi2_flat[i];
        return F;
    }

    void createBuffers(){
        VkDeviceSize sz=(VkDeviceSize)total2d*sizeof(float);
        VkDeviceSize szXS=(VkDeviceSize)total2d*9*sizeof(float);
        VkDeviceSize szZ=(VkDeviceSize)total2d*sizeof(int);

        auto mkG=[&](VkDeviceSize s,VkBuffer& b,VkDeviceMemory& m){
            _ctx->createBuffer(s,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                b,m);
        };

        auto mkH=[&](VkDeviceSize s,VkBuffer& b,VkDeviceMemory& m,VkBufferUsageFlags x=0){
            _ctx->createBuffer(s,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | x,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                b,m);
        };

        mkG(sz,_bufPhi1A,_memPhi1A); mkG(sz,_bufPhi1B,_memPhi1B);
        mkG(sz,_bufPhi2A,_memPhi2A); mkG(sz,_bufPhi2B,_memPhi2B);
        mkH(szXS,_bufXS,_memXS); mkH(szZ,_bufZone,_memZone);
        mkH(sizeof(NeutronParams),_bufParams,_memParams,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        _ctx->createBuffer(sz*2,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _rbBuf,_rbMem);

        uploadXS(); uploadZones(); uploadFlux(); uploadParams();
    }

    void uploadXS(){
        void* p; VkDeviceSize s=(VkDeviceSize)total2d*9*sizeof(float);
        vkMapMemory(_ctx->device,_memXS,0,s,0,&p);
        memcpy(p,xs_flat.data(),s);
        vkUnmapMemory(_ctx->device,_memXS);
    }

    void uploadZones(){
        void* p; VkDeviceSize s=(VkDeviceSize)total2d*sizeof(int);
        vkMapMemory(_ctx->device,_memZone,0,s,0,&p);
        memcpy(p,_zone_flat.data(),s);
        vkUnmapMemory(_ctx->device,_memZone);
    }

    void uploadParams(){
        void* p;
        vkMapMemory(_ctx->device,_memParams,0,sizeof(NeutronParams),0,&p);
        memcpy(p,&params,sizeof(NeutronParams));
        vkUnmapMemory(_ctx->device,_memParams);
    }

    void uploadFlux(){
        VkDeviceSize sz=(VkDeviceSize)total2d*sizeof(float);

        auto stg=[&](VkBuffer dst,const std::vector<float>& d){
            VkBuffer s; VkDeviceMemory sm;
            _ctx->createBuffer(sz,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                s,sm);

            void* p;
            vkMapMemory(_ctx->device,sm,0,sz,0,&p);
            memcpy(p,d.data(),sz);
            vkUnmapMemory(_ctx->device,sm);

            auto cb=_ctx->beginOneShot();
            VkBufferCopy bc{0,0,sz};
            vkCmdCopyBuffer(cb,s,dst,1,&bc);
            _ctx->endOneShot(cb);

            vkDestroyBuffer(_ctx->device,s,nullptr);
            vkFreeMemory(_ctx->device,sm,nullptr);
        };

        stg(_bufPhi1A,phi1_flat);
        stg(_bufPhi2A,phi2_flat);
    }

    void createPipeline(){
        auto spv=loadSPV("compute/shaders/neutron.spv");

        VkShaderModuleCreateInfo sc{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sc.codeSize=spv.size()*4;
        sc.pCode=spv.data();
        VK_CHECK(vkCreateShaderModule(_ctx->device,&sc,nullptr,&_shaderModule));

        // --- Correction du warning : i en uint32_t ---
        VkDescriptorSetLayoutBinding b[7]={};
        for (uint32_t i=0;i<6;++i) {
            b[i] = { i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                     VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }
        b[6] = { 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo di{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        di.bindingCount=7;
        di.pBindings=b;
        VK_CHECK(vkCreateDescriptorSetLayout(_ctx->device,&di,nullptr,&_descSetLayout));

        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount=1;
        pl.pSetLayouts=&_descSetLayout;
        VK_CHECK(vkCreatePipelineLayout(_ctx->device,&pl,nullptr,&_pipelineLayout));

        VkPipelineShaderStageCreateInfo st{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        st.stage=VK_SHADER_STAGE_COMPUTE_BIT;
        st.module=_shaderModule;
        st.pName="main";

        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage=st;
        ci.layout=_pipelineLayout;
        VK_CHECK(vkCreateComputePipelines(_ctx->device,VK_NULL_HANDLE,1,&ci,nullptr,&_pipeline));
    }

    void createDescriptors(){
        VkDescriptorSetLayout la[2]={_descSetLayout,_descSetLayout};

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool=_ctx->descPool;
        ai.descriptorSetCount=2;
        ai.pSetLayouts=la;

        VkDescriptorSet ss[2];
        VK_CHECK(vkAllocateDescriptorSets(_ctx->device,&ai,ss));

        _descSetAB=ss[0];
        _descSetBA=ss[1];

        wDesc(_descSetAB,_bufPhi1A,_bufPhi2A,_bufPhi1B,_bufPhi2B);
        wDesc(_descSetBA,_bufPhi1B,_bufPhi2B,_bufPhi1A,_bufPhi2A);
    }

    void wDesc(VkDescriptorSet ds,
            VkBuffer p1i, VkBuffer p2i,
            VkBuffer p1o, VkBuffer p2o)
    {
        VkDescriptorBufferInfo bi[7] = {
            { p1i,       0, VK_WHOLE_SIZE },
            { p2i,       0, VK_WHOLE_SIZE },
            { p1o,       0, VK_WHOLE_SIZE },
            { p2o,       0, VK_WHOLE_SIZE },
            { _bufXS,    0, VK_WHOLE_SIZE },
            { _bufZone,  0, VK_WHOLE_SIZE },
            { _bufParams,0, VK_WHOLE_SIZE }
        };

        VkWriteDescriptorSet wr[7] = {};
        for (int i = 0; i < 7; ++i) {
            wr[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wr[i].dstSet          = ds;
            wr[i].dstBinding      = i;
            wr[i].descriptorCount = 1;
            wr[i].descriptorType  =
                (i < 6) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                        : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wr[i].pBufferInfo     = &bi[i];
        }
        vkUpdateDescriptorSets(_ctx->device, 7, wr, 0, nullptr);
    }

    void runOneStep(){
        VkDescriptorSet ds = _pingPong ? _descSetBA : _descSetAB;
        VkCommandBuffer cb = _ctx->beginOneShot();

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdDispatch(cb,
                      ((uint32_t)grid_cols + 7) / 8,
                      ((uint32_t)grid_rows + 7) / 8,
                      1);

        VkMemoryBarrier bar{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            1, &bar,
            0, nullptr,
            0, nullptr);

        _ctx->endOneShot(cb);
    }

    void readback(){
        VkBuffer sp1 = _pingPong ? _bufPhi1A : _bufPhi1B;
        VkBuffer sp2 = _pingPong ? _bufPhi2A : _bufPhi2B;
        VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(float);

        auto cb = _ctx->beginOneShot();
        VkBufferCopy b1{0, 0,   sz};
        VkBufferCopy b2{0, sz,  sz};
        vkCmdCopyBuffer(cb, sp1, _rbBuf, 1, &b1);
        vkCmdCopyBuffer(cb, sp2, _rbBuf, 1, &b2);
        _ctx->endOneShot(cb);

        void* p;
        vkMapMemory(_ctx->device, _rbMem, 0, sz*2, 0, &p);
        memcpy(phi1_flat.data(), p,        sz);
        memcpy(phi2_flat.data(), (char*)p + sz, sz);
        vkUnmapMemory(_ctx->device, _rbMem);
    }

    std::vector<uint32_t> loadSPV(const std::string& path){
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            throw std::runtime_error("SPIR-V introuvable: " + path);

        size_t sz = f.tellg();
        f.seekg(0);
        std::vector<uint32_t> buf(sz / 4);
        f.read(reinterpret_cast<char*>(buf.data()), sz);
        return buf;
    }
};
