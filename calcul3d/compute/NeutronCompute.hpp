#pragma once
// ============================================================
//  NeutronCompute.hpp  —  Diffusion neutronique 2 groupes
//
//  ARCHITECTURE HYBRIDE :
//    - CPU (toujours dispo) : même algo FDM que le shader GLSL
//    - GPU Vulkan (optionnel) : accélération si neutron.spv compilé
//
//  Le CPU tourne SYSTÉMATIQUEMENT si le GPU échoue.
//  neutronAvailable = true dès que init() réussit sur CPU.
//  gpuAccel = true si en plus le pipeline Vulkan est chargé.
//
//  Equations 2 groupes (FDM explicite) :
//    phi_g^(n+1) = phi_g^n + dt*v_g * [
//        ∇·(D_g∇phi_g) - SigR_g·phi_g
//      + SigS12·phi1 (groupe 2 seulement)
//      + (chi_g/k)·Σ(nuSigF·phi)
//    ]
//  k_eff mis à jour par itération de puissance CPU chaque step.
// ============================================================
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <algorithm>
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
    bool  ready    = false;
    bool  gpuAccel = false;   // true si pipeline Vulkan actif
    float k_eff    = 1.0f;
    std::vector<float> phi1_flat, phi2_flat, phi_total, xs_flat;
    int grid_cols=0, grid_rows=0, total2d=0;
    NeutronParams params{};

    struct ModNode  { int i2d; float rho_rel; };
    struct CtrlRod  { int row, col; float insertFraction; };
    std::vector<ModNode> _moderatorNodes;
    std::vector<int>     _reflectorNodes;
    std::vector<CtrlRod> _controlRods;

    void setControlRod(int row, int col, float ins) {
        for (auto& c : _controlRods)
            if (c.row==row && c.col==col) { c.insertFraction=ins; return; }
        _controlRods.push_back({row,col,ins});
    }

    // --------------------------------------------------------
    //  init — CPU toujours, Vulkan GPU optionnel
    // --------------------------------------------------------
    bool init(VulkanContext& ctx, const GridData& grid,
              ReactorType rt, float enrichment, float T0=300.0f)
    {
        _ctx = &ctx; _rt = rt; _epsilon = enrichment;
        grid_cols = grid.cols; grid_rows = grid.rows;
        total2d   = grid_cols * grid_rows;

        float dx_cm = (grid.dims.width + grid.dims.spacing) * 100.0f;
        // dt Fourier 2D : dt * v * 4D / dx² ≤ 1
        // valeur conservatrice, sera affinée par technologie
        float D_typ = 1.5f;
        float v_typ = 1.0f;
        // Contrainte diffusion Fourier
        float dt_diff = dx_cm * dx_cm / (4.0f * D_typ * v_typ) * 0.45f;
        // Contrainte source fission (semi-implicite tolère dt plus grand,
        // mais on limite quand même pour la précision de k_eff)
        // dt_src = 0.45 / (v1 * nuSigF2_max_attendu)
        // nuSigF2 max ~ 0.35 cm-1 (REP nominal) → dt_src ~ 1.3 s
        float dt_src = 0.45f / (v_typ * 0.35f);
        float dt_fourier = fminf(dt_diff, dt_src);
        dt_fourier = fmaxf(1e-4f, dt_fourier);

        params = {dt_fourier, dx_cm, 1.0f, 1.0f,
                  grid_cols, grid_rows, total2d,
                  1.0f, 0.1f, T0, {0,0}};

        phi1_flat.assign(total2d, 0.0f);
        phi2_flat.assign(total2d, 0.0f);
        phi_total.assign(total2d, 0.0f);
        xs_flat.assign(total2d * 9, 0.0f);
        _zone_flat.assign(total2d, 0);
        _phi1_new.assign(total2d, 0.0f);
        _phi2_new.assign(total2d, 0.0f);

        // Profil initial cosinus
        float R = sqrtf(grid.offsetX*grid.offsetX + grid.offsetZ*grid.offsetZ);
        if (R < 0.1f) R = 1.0f;
        for (const auto& c : grid.cubes) {
            int i = c.row * grid_cols + c.col_idx;
            if (i < 0 || i >= total2d) continue;
            float r = sqrtf(c.pos.x*c.pos.x + c.pos.z*c.pos.z);
            float v = cosf(fminf((float)M_PI*r/(2.0f*R*1.05f), (float)M_PI*0.499f));
            phi1_flat[i] = v * 0.1f;
            phi2_flat[i] = v * 0.9f;
            _zone_flat[i] = 1;
        }

        _moderatorNodes.clear(); _reflectorNodes.clear(); _controlRods.clear();
        for (const auto& zn : grid.zoneNodes) {
            if (zn.row<0||zn.row>=grid_rows||zn.col<0||zn.col>=grid_cols) continue;
            int i = zn.row*grid_cols + zn.col;
            if (zn.zone==NodeZone::MODERATOR)
                { _moderatorNodes.push_back({i,zn.param}); _zone_flat[i]=2; }
            if (zn.zone==NodeZone::REFLECTOR)
                { _reflectorNodes.push_back(i); _zone_flat[i]=3; }
            if (zn.zone==NodeZone::CONTROL_ROD)
                { _controlRods.push_back({zn.row,zn.col,zn.param}); _zone_flat[i]=4; }
        }

        rebuildXS(grid, T0);
        ready = true;  // CPU toujours prêt

        // Initialiser phi_total depuis phi1/phi2 initiaux (profil cosinus)
        // SANS ça, recalcFlux() dans le reset lit phi_total=0 → q_vol=0 → T froid
        {
            float mx = 1e-15f;
            for (int i = 0; i < total2d; ++i)
                mx = fmaxf(mx, phi1_flat[i] + phi2_flat[i]);
            for (int i = 0; i < total2d; ++i)
                phi_total[i] = (phi1_flat[i] + phi2_flat[i]) / mx;
        }

        // Tentative GPU (optionnelle)
        gpuAccel = false;
        try {
            _initGPU();
            gpuAccel = true;
            std::cout << "[NeutronCompute] GPU Vulkan actif — "
                      << NeutronCrossSection::reactorName(rt)
                      << "  eps=" << enrichment*100.0f << "%\n";
        } catch (const std::exception& e) {
            std::cout << "[NeutronCompute] CPU mode (GPU indispo: "
                      << e.what() << ")\n";
        }

        std::cout << "[NeutronCompute] OK (CPU"
                  << (gpuAccel ? "+GPU" : " seul") << ") — "
                  << NeutronCrossSection::reactorName(rt)
                  << "  eps=" << enrichment*100.0f
                  << "%  dx=" << dx_cm << "cm  dt=" << dt_fourier*1000.0f << "ms\n";
        return true;  // toujours true (CPU dispo)
    }

    // --------------------------------------------------------
    //  rebuildXS — sections efficaces depuis T actuelles
    // --------------------------------------------------------
    void rebuildXS(const GridData& grid, float T0=300.0f, float rho_mod=1.0f) {
        for (int i = 0; i < total2d; ++i) writeXS(i, XS2G{});
        for (const auto& c : grid.cubes) {
            int i = c.row*grid_cols + c.col_idx;
            if (i<0||i>=total2d) continue;
            float T = (c.temperature > 100.0f) ? c.temperature : T0;
            writeXS(i, NeutronCrossSection::fuel(_rt, _epsilon, T, rho_mod));
            _zone_flat[i] = 1;
        }
        for (const auto& mn : _moderatorNodes)
            writeXS(mn.i2d, NeutronCrossSection::moderator(_rt, mn.rho_rel));
        for (int i : _reflectorNodes)
            writeXS(i, NeutronCrossSection::reflector(_rt));
        for (const auto& cr : _controlRods) {
            int i = cr.row*grid_cols + cr.col;
            if (i>=0&&i<total2d) writeXS(i, NeutronCrossSection::controlRod(cr.insertFraction));
        }
        // Upload GPU si actif
        if (gpuAccel) { try { _uploadXS(); _uploadZones(); } catch(...){} }
    }

    // --------------------------------------------------------
    //  step — nDiff pas de diffusion, nPower itérations puissance
    // --------------------------------------------------------
    void step(int nDiff, int nPower=3) {
        if (!ready) return;
        float fp = computeF();
        for (int pi = 0; pi < nPower; ++pi) {
            for (int di = 0; di < nDiff; ++di) {
                if (gpuAccel) _stepGPU();
                else           _stepCPU();
            }
            if (gpuAccel) _readbackGPU();
            float fn = computeF();
            if (fp > 1e-15f) k_eff *= (fn / fp);
            k_eff = fmaxf(0.001f, fminf(k_eff, 10.0f));
            fp = (fn > 1e-15f) ? fn : fp;
            params.k_eff = k_eff;
            if (gpuAccel) { try { _uploadParams(); } catch(...){} }
        }
        // Normaliser phi_total
        float mx = 1e-15f;
        for (int i = 0; i < total2d; ++i)
            mx = fmaxf(mx, phi1_flat[i] + phi2_flat[i]);
        for (int i = 0; i < total2d; ++i)
            phi_total[i] = (phi1_flat[i] + phi2_flat[i]) / mx;
    }

    void applyToGrid(GridData& grid) {
        for (auto& c : grid.cubes) {
            int i = c.row*grid_cols + c.col_idx;
            if (i>=0&&i<total2d) c.flux = phi_total[i];
        }
    }

    void cleanup() {
        if (_ctx && gpuAccel) _cleanupGPU();
        _moderatorNodes.clear(); _reflectorNodes.clear(); _controlRods.clear();
        ready = false; gpuAccel = false; k_eff = 1.0f;
    }

private:
    VulkanContext* _ctx = nullptr;
    ReactorType    _rt  = ReactorType::REP;
    float          _epsilon = 0.035f;
    std::vector<int>   _zone_flat;
    std::vector<float> _phi1_new, _phi2_new;  // buffer CPU temporaire

    // ============================================================
    //  CPU SOLVER — FDM explicite identique au shader GLSL
    // ============================================================
    void _stepCPU() {
        const float dt  = params.dt;
        const float dx  = params.dx;
        const float v1  = params.v1;
        const float v2  = params.v2;
        const float k   = k_eff;
        const float idx2 = 1.0f / (dx * dx);
        const int   NC  = grid_cols;
        const int   NR  = grid_rows;

        for (int row = 0; row < NR; ++row) {
            for (int col = 0; col < NC; ++col) {
                int i = row * NC + col;

                // Bord ou zone vide → Dirichlet phi=0
                if (_zone_flat[i] == 0) {
                    _phi1_new[i] = 0.0f;
                    _phi2_new[i] = 0.0f;
                    continue;
                }

                float p1 = phi1_flat[i];
                float p2 = phi2_flat[i];

                // Sections efficaces locales
                float D1    = xs_flat[i*9+0], D2    = xs_flat[i*9+1];
                float SR1   = xs_flat[i*9+2], SR2   = xs_flat[i*9+3];
                float SS12  = xs_flat[i*9+4];
                float nSF1  = xs_flat[i*9+5], nSF2  = xs_flat[i*9+6];
                float chi1  = xs_flat[i*9+7], chi2  = xs_flat[i*9+8];

                // Source de fission
                float Fsrc = nSF1*p1 + nSF2*p2;

                // Laplacien FDM 5 points avec moyenne harmonique des D (interface)
                auto getphi = [&](int r2, int c2, int g) -> float {
                    if (r2<0||r2>=NR||c2<0||c2>=NC) return 0.0f;
                    int j = r2*NC+c2;
                    if (_zone_flat[j]==0) return 0.0f;
                    return (g==0) ? phi1_flat[j] : phi2_flat[j];
                };
                auto getD = [&](int r2, int c2, int g) -> float {
                    if (r2<0||r2>=NR||c2<0||c2>=NC) return 0.0f;
                    int j = r2*NC+c2;
                    return xs_flat[j*9+g];  // D1 ou D2
                };

                // Diffusion groupe 1
                float lap1 = 0.0f;
                {
                    // voisins E W N S
                    int dr[4]={0,0,-1,1}, dc[4]={1,-1,0,0};
                    for (int d=0;d<4;++d) {
                        float Dn = getD(row+dr[d], col+dc[d], 0);
                        float D_eff = (D1+Dn>1e-10f) ? 2.0f*D1*Dn/(D1+Dn) : 0.0f;
                        float phi_n = getphi(row+dr[d], col+dc[d], 0);
                        lap1 += D_eff * (phi_n - p1);
                    }
                    lap1 *= idx2;
                }
                // Diffusion groupe 2
                float lap2 = 0.0f;
                {
                    int dr[4]={0,0,-1,1}, dc[4]={1,-1,0,0};
                    for (int d=0;d<4;++d) {
                        float Dn = getD(row+dr[d], col+dc[d], 1);
                        float D_eff = (D2+Dn>1e-10f) ? 2.0f*D2*Dn/(D2+Dn) : 0.0f;
                        float phi_n = getphi(row+dr[d], col+dc[d], 1);
                        lap2 += D_eff * (phi_n - p2);
                    }
                    lap2 *= idx2;
                }

                // Schéma SEMI-IMPLICITE: absorption traitée implicitement
                // phi^(n+1) = [phi^n + dt*v*(L + source_fission)] / (1 + dt*v*SigR)
                // Élimine l'instabilité du terme d'absorption/fission (stiffness)
                float src1 = (chi1/k) * Fsrc;
                float src2 = (chi2/k) * Fsrc + SS12*p1;
                _phi1_new[i] = fmaxf(0.0f, (p1 + dt*v1*(lap1 + src1)) / (1.0f + dt*v1*SR1));
                _phi2_new[i] = fmaxf(0.0f, (p2 + dt*v2*(lap2 + src2)) / (1.0f + dt*v2*SR2));
            }
        }
        std::swap(phi1_flat, _phi1_new);
        std::swap(phi2_flat, _phi2_new);
    }

    float computeF() {
        float F = 0.0f;
        for (int i = 0; i < total2d; ++i)
            F += xs_flat[i*9+5]*phi1_flat[i] + xs_flat[i*9+6]*phi2_flat[i];
        return F;
    }

    void writeXS(int i, const XS2G& xs) {
        int b = i*9;
        xs_flat[b+0]=xs.D[0];    xs_flat[b+1]=xs.D[1];
        xs_flat[b+2]=xs.SigR[0]; xs_flat[b+3]=xs.SigR[1];
        xs_flat[b+4]=xs.SigS12;
        xs_flat[b+5]=xs.nuSigF[0]; xs_flat[b+6]=xs.nuSigF[1];
        xs_flat[b+7]=xs.chi[0];    xs_flat[b+8]=xs.chi[1];
    }

    // ============================================================
    //  GPU Vulkan (optionnel)
    // ============================================================
    bool _pingPong = false;
    VkBuffer       _bufPhi1A=VK_NULL_HANDLE, _bufPhi1B=VK_NULL_HANDLE;
    VkDeviceMemory _memPhi1A=VK_NULL_HANDLE, _memPhi1B=VK_NULL_HANDLE;
    VkBuffer       _bufPhi2A=VK_NULL_HANDLE, _bufPhi2B=VK_NULL_HANDLE;
    VkDeviceMemory _memPhi2A=VK_NULL_HANDLE, _memPhi2B=VK_NULL_HANDLE;
    VkBuffer       _bufXS=VK_NULL_HANDLE,    _bufZone=VK_NULL_HANDLE;
    VkDeviceMemory _memXS=VK_NULL_HANDLE,    _memZone=VK_NULL_HANDLE;
    VkBuffer       _bufParams=VK_NULL_HANDLE, _rbBuf=VK_NULL_HANDLE;
    VkDeviceMemory _memParams=VK_NULL_HANDLE, _rbMem=VK_NULL_HANDLE;
    VkShaderModule        _shaderModule    = VK_NULL_HANDLE;
    VkPipelineLayout      _pipelineLayout  = VK_NULL_HANDLE;
    VkPipeline            _pipeline        = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descSetLayout   = VK_NULL_HANDLE;
    VkDescriptorSet       _descSetAB=VK_NULL_HANDLE, _descSetBA=VK_NULL_HANDLE;

    void _initGPU() {
        VkDeviceSize sz   = (VkDeviceSize)total2d * sizeof(float);
        VkDeviceSize szXS = (VkDeviceSize)total2d * 9 * sizeof(float);
        VkDeviceSize szZ  = (VkDeviceSize)total2d * sizeof(int);

        auto mkG=[&](VkDeviceSize s, VkBuffer& b, VkDeviceMemory& m){
            _ctx->createBuffer(s,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, b, m);};
        auto mkH=[&](VkDeviceSize s, VkBuffer& b, VkDeviceMemory& m, VkBufferUsageFlags x=0){
            _ctx->createBuffer(s,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|x,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, b, m);};

        mkG(sz,_bufPhi1A,_memPhi1A); mkG(sz,_bufPhi1B,_memPhi1B);
        mkG(sz,_bufPhi2A,_memPhi2A); mkG(sz,_bufPhi2B,_memPhi2B);
        mkH(szXS,_bufXS,_memXS);
        mkH(szZ, _bufZone,_memZone);
        mkH(sizeof(NeutronParams),_bufParams,_memParams,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        _ctx->createBuffer(sz*2, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _rbBuf, _rbMem);

        _uploadXS(); _uploadZones(); _uploadFlux(); _uploadParams();
        _createPipeline();
        _createDescriptors();
    }

    void _stepGPU() {
        VkDescriptorSet ds = _pingPong ? _descSetBA : _descSetAB;
        VkCommandBuffer cb = _ctx->beginOneShot();
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
            _pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdDispatch(cb, ((uint32_t)grid_cols+7)/8, ((uint32_t)grid_rows+7)/8, 1);
        VkMemoryBarrier bar{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &bar, 0, nullptr, 0, nullptr);
        _ctx->endOneShot(cb);
        _pingPong = !_pingPong;
    }

    void _readbackGPU() {
        VkBuffer sp1 = _pingPong ? _bufPhi1A : _bufPhi1B;
        VkBuffer sp2 = _pingPong ? _bufPhi2A : _bufPhi2B;
        VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(float);
        auto cb = _ctx->beginOneShot();
        VkBufferCopy b1{0,0,sz}, b2{0,sz,sz};
        vkCmdCopyBuffer(cb, sp1, _rbBuf, 1, &b1);
        vkCmdCopyBuffer(cb, sp2, _rbBuf, 1, &b2);
        _ctx->endOneShot(cb);
        void* p; vkMapMemory(_ctx->device, _rbMem, 0, sz*2, 0, &p);
        memcpy(phi1_flat.data(), p, sz);
        memcpy(phi2_flat.data(), (char*)p+sz, sz);
        vkUnmapMemory(_ctx->device, _rbMem);
    }

    void _uploadXS() {
        VkDeviceSize s = (VkDeviceSize)total2d*9*sizeof(float);
        void* p; vkMapMemory(_ctx->device,_memXS,0,s,0,&p);
        memcpy(p,xs_flat.data(),s); vkUnmapMemory(_ctx->device,_memXS);
    }
    void _uploadZones() {
        VkDeviceSize s = (VkDeviceSize)total2d*sizeof(int);
        void* p; vkMapMemory(_ctx->device,_memZone,0,s,0,&p);
        memcpy(p,_zone_flat.data(),s); vkUnmapMemory(_ctx->device,_memZone);
    }
    void _uploadParams() {
        void* p; vkMapMemory(_ctx->device,_memParams,0,sizeof(NeutronParams),0,&p);
        memcpy(p,&params,sizeof(NeutronParams)); vkUnmapMemory(_ctx->device,_memParams);
    }
    void _uploadFlux() {
        VkDeviceSize sz = (VkDeviceSize)total2d*sizeof(float);
        auto stg=[&](VkBuffer dst, const std::vector<float>& d){
            VkBuffer s; VkDeviceMemory sm;
            _ctx->createBuffer(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, s, sm);
            void* p; vkMapMemory(_ctx->device,sm,0,sz,0,&p);
            memcpy(p,d.data(),sz); vkUnmapMemory(_ctx->device,sm);
            auto cb=_ctx->beginOneShot();
            VkBufferCopy bc{0,0,sz}; vkCmdCopyBuffer(cb,s,dst,1,&bc);
            _ctx->endOneShot(cb);
            vkDestroyBuffer(_ctx->device,s,nullptr);
            vkFreeMemory(_ctx->device,sm,nullptr);
        };
        stg(_bufPhi1A, phi1_flat); stg(_bufPhi2A, phi2_flat);
    }
    void _createPipeline() {
        auto spv = _loadSPV("compute/shaders/neutron.spv");
        VkShaderModuleCreateInfo sc{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sc.codeSize = spv.size()*4; sc.pCode = spv.data();
        VK_CHECK(vkCreateShaderModule(_ctx->device,&sc,nullptr,&_shaderModule));
        VkDescriptorSetLayoutBinding b[7]={};
        for(uint32_t i=0;i<6;++i) b[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        b[6]={6,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo di{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        di.bindingCount=7; di.pBindings=b;
        VK_CHECK(vkCreateDescriptorSetLayout(_ctx->device,&di,nullptr,&_descSetLayout));
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount=1; pl.pSetLayouts=&_descSetLayout;
        VK_CHECK(vkCreatePipelineLayout(_ctx->device,&pl,nullptr,&_pipelineLayout));
        VkPipelineShaderStageCreateInfo st{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        st.stage=VK_SHADER_STAGE_COMPUTE_BIT; st.module=_shaderModule; st.pName="main";
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage=st; ci.layout=_pipelineLayout;
        VK_CHECK(vkCreateComputePipelines(_ctx->device,VK_NULL_HANDLE,1,&ci,nullptr,&_pipeline));
    }
    void _createDescriptors() {
        VkDescriptorSetLayout la[2]={_descSetLayout,_descSetLayout};
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool=_ctx->descPool; ai.descriptorSetCount=2; ai.pSetLayouts=la;
        VkDescriptorSet ss[2]; VK_CHECK(vkAllocateDescriptorSets(_ctx->device,&ai,ss));
        _descSetAB=ss[0]; _descSetBA=ss[1];
        _wDesc(_descSetAB,_bufPhi1A,_bufPhi2A,_bufPhi1B,_bufPhi2B);
        _wDesc(_descSetBA,_bufPhi1B,_bufPhi2B,_bufPhi1A,_bufPhi2A);
    }
    void _wDesc(VkDescriptorSet ds, VkBuffer p1i, VkBuffer p2i, VkBuffer p1o, VkBuffer p2o){
        VkDescriptorBufferInfo bi[7]={
            {p1i,0,VK_WHOLE_SIZE},{p2i,0,VK_WHOLE_SIZE},
            {p1o,0,VK_WHOLE_SIZE},{p2o,0,VK_WHOLE_SIZE},
            {_bufXS,0,VK_WHOLE_SIZE},{_bufZone,0,VK_WHOLE_SIZE},
            {_bufParams,0,VK_WHOLE_SIZE}};
        VkWriteDescriptorSet wr[7]={};
        for(int i=0;i<7;++i){
            wr[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[i].dstSet=ds; wr[i].dstBinding=i; wr[i].descriptorCount=1;
            wr[i].descriptorType=(i<6)?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wr[i].pBufferInfo=&bi[i];}
        vkUpdateDescriptorSets(_ctx->device,7,wr,0,nullptr);
    }
    void _cleanupGPU() {
        auto del=[&](VkBuffer b,VkDeviceMemory m){
            if(b) vkDestroyBuffer(_ctx->device,b,nullptr);
            if(m) vkFreeMemory(_ctx->device,m,nullptr);};
        del(_bufPhi1A,_memPhi1A); del(_bufPhi1B,_memPhi1B);
        del(_bufPhi2A,_memPhi2A); del(_bufPhi2B,_memPhi2B);
        del(_bufXS,_memXS); del(_bufZone,_memZone);
        del(_bufParams,_memParams); del(_rbBuf,_rbMem);
        if(_pipeline)       vkDestroyPipeline(_ctx->device,_pipeline,nullptr);
        if(_pipelineLayout) vkDestroyPipelineLayout(_ctx->device,_pipelineLayout,nullptr);
        if(_descSetLayout)  vkDestroyDescriptorSetLayout(_ctx->device,_descSetLayout,nullptr);
        if(_shaderModule)   vkDestroyShaderModule(_ctx->device,_shaderModule,nullptr);
        _pipeline        = VK_NULL_HANDLE;
        _pipelineLayout  = VK_NULL_HANDLE;
        _descSetLayout   = VK_NULL_HANDLE;
        _shaderModule    = VK_NULL_HANDLE;
        _bufPhi1A=_bufPhi1B=_bufPhi2A=_bufPhi2B=VK_NULL_HANDLE;
        _bufXS=_bufZone=_bufParams=_rbBuf=VK_NULL_HANDLE;
        _memPhi1A=_memPhi1B=_memPhi2A=_memPhi2B=VK_NULL_HANDLE;
        _memXS=_memZone=_memParams=_rbMem=VK_NULL_HANDLE;
    }
    std::vector<uint32_t> _loadSPV(const std::string& path) {
        std::ifstream f(path, std::ios::binary|std::ios::ate);
        if (!f.is_open()) throw std::runtime_error("SPIR-V introuvable: "+path);
        size_t sz = f.tellg(); f.seekg(0);
        std::vector<uint32_t> buf(sz/4);
        f.read(reinterpret_cast<char*>(buf.data()), sz);
        return buf;
    }
};