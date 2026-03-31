#pragma once
// ============================================================
//  NeutronCompute.hpp  v2.0
//
//  NOUVEAUTÉS v2 :
//    1. XS en SoA (9 buffers séparés au lieu d'un seul stride-9)
//       → accès GPU coalescé → ×9 bande passante effective
//    2. Shader neutron_fvm.comp (FVM ordre 2 + précurseurs in-shader)
//    3. Précurseurs dans des buffers GPU séparés (ping-pong)
//    4. MeshConfig intégré : dx/dz RÉELS passés au shader
//    5. NeutronParams enrichi (beta_tot + 6×{βᵢ,λᵢ} inline)
//    6. GPU actif par défaut — fallback CPU si init échoue
//
//  ARCHITECTURE :
//    Pipeline A — neutron_fvm.comp
//      Bindings : phi1A/B, phi2A/B, 9×XS_SoA, zone,
//                 prec_in/out, NeutronParams (uniform)
//    Pipeline B — neutron_reduce.comp
//      Bindings : phi1, phi2, nuSF1, nuSF2, zone, partial_sums,
//                 ReduceParams
//
//  CPU FALLBACK :
//    Identique à v1 — semi-implicite 2G avec précurseurs 6G.
//    Activé si gpuAccel=false ou si init GPU échoue.
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
#include "../physics/PrecursorData.hpp"
#include "../physics/XenonModel.hpp"

// ── Uniform buffer NeutronParams ────────────────────────────
// IMPORTANT : doit être multiple de 16 octets (std140)
struct NeutronParams {
    float dt;
    float dx;         // cm — pas spatial X
    float dz;         // cm — pas spatial Z
    float k_eff;

    int   grid_cols;
    int   grid_rows;
    int   total2d;
    float v1;

    float v2;
    float beta_tot;   // Σβᵢ
    float T_inlet;
    float _pad0;

    // 6 groupes précurseurs Keepin inline (évite un buffer supplémentaire)
    float beta0; float lambda0;
    float beta1; float lambda1;
    float beta2; float lambda2;
    float beta3; float lambda3;
    float beta4; float lambda4;
    float beta5; float lambda5;

    int _pad[2];      // aligne à 16 octets
};

struct ReduceParams {
    int total2d;
    int _pad[3];
};

// ── Helper macro ────────────────────────────────────────────
#ifndef VK_CHECK
#define VK_CHECK(call) do { \
    VkResult _r=(call); \
    if(_r!=VK_SUCCESS){ \
        throw std::runtime_error(std::string("[VK] ")+#call+" = "+std::to_string(_r)); \
    } \
} while(0)
#endif

class NeutronCompute {
public:
    bool  ready    = false;
    bool  gpuAccel = false;
    float k_eff    = 1.0f;
    float reactivity  = 0.0f;
    float power_rel   = 1.0f;

    // Données CPU (toujours maintenues pour le fallback et le rendu)
    std::vector<float> phi1_flat, phi2_flat, phi_total;
    std::vector<float> precursor_flat;  // [total2d × 6]

    // XS SoA — 9 vecteurs séparés (layout GPU-friendly)
    std::vector<float> xs_D1, xs_D2, xs_SigR1, xs_SigR2, xs_SigS12;
    std::vector<float> xs_nuSF1, xs_nuSF2, xs_chi1, xs_chi2;

    std::vector<int>   zone_flat;

    // ── Modèle Xénon/Iode/Samarium ───────────────────────────
    XenonModel xenon;
    bool xenonActive = false;

    // ── Paramètres caloporteur/bore (mis à jour par main) ────
    float T_mod_avg    = 290.0f;   // °C — T modérateur moyen
    float C_bore_ppm   = 0.0f;    // ppm — concentration bore (REP)

    int grid_cols=0, grid_rows=0, total2d=0;
    int sub_xy=1;   // subdivisions XZ par assemblage
    int n_assy_cols=0, n_assy_rows=0;  // dimensions grille assemblage
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

    // ── init ────────────────────────────────────────────────
    bool init(VulkanContext& ctx, const GridData& grid,
              ReactorType rt, float enrichment, float T0=300.0f)
    {
        _ctx = &ctx; _rt = rt; _epsilon = enrichment;
        grid_cols   = grid.cols;   // = n_assy_cols * sub_xy
        grid_rows   = grid.rows;   // = n_assy_rows * sub_xy
        total2d     = grid.total2d();
        // Déduire sub_xy depuis la grille
        // cube.col_idx max = n_assy_cols-1, grid_cols = n_assy_cols * sub_xy
        n_assy_cols = 0; n_assy_rows = 0;
        for (const auto& cu : grid.cubes) {
            n_assy_cols = std::max(n_assy_cols, cu.col_idx+1);
            n_assy_rows = std::max(n_assy_rows, cu.row+1);
        }
        if (n_assy_cols < 1) n_assy_cols = grid_cols;
        if (n_assy_rows < 1) n_assy_rows = grid_rows;
        sub_xy = (n_assy_cols > 0) ? grid_cols / n_assy_cols : 1;
        if (sub_xy < 1) sub_xy = 1;

        // Pas spatiaux en cm
        float dx_cm = grid.dx_m * 100.0f;
        float dz_cm = grid.dz_m * 100.0f;

        // Calcul dt de stabilité
        float D_typ = 1.5f, v_typ = 1.0f;
        float dt_diff = std::min(dx_cm,dz_cm)*std::min(dx_cm,dz_cm)
                      / (4.0f * D_typ * v_typ) * 0.45f;
        float dt_src  = 0.45f / (v_typ * 0.35f);
        float dt0     = std::max(1e-4f, std::min(dt_diff, dt_src));

        // Données précurseurs Keepin
        const auto& pg = PrecursorData::get(rt);
        float beta_tot = PrecursorData::beta_total(rt);

        params = {};
        params.dt      = dt0;
        params.dx      = dx_cm;
        params.dz      = dz_cm;
        params.k_eff   = 1.0f;
        params.grid_cols = grid_cols;
        params.grid_rows = grid_rows;
        params.total2d   = total2d;
        params.v1      = 1.0f;
        params.v2      = 0.1f;
        params.beta_tot = beta_tot;
        params.T_inlet  = T0;
        // Précurseurs inline
        params.beta0=pg[0].beta;  params.lambda0=pg[0].lambda;
        params.beta1=pg[1].beta;  params.lambda1=pg[1].lambda;
        params.beta2=pg[2].beta;  params.lambda2=pg[2].lambda;
        params.beta3=pg[3].beta;  params.lambda3=pg[3].lambda;
        params.beta4=pg[4].beta;  params.lambda4=pg[4].lambda;
        params.beta5=pg[5].beta;  params.lambda5=pg[5].lambda;

        // Alloc CPU
        phi1_flat.assign(total2d, 0.0f);
        phi2_flat.assign(total2d, 0.0f);
        phi_total.assign(total2d, 0.0f);
        zone_flat.assign(total2d, 0);
        _phi1_new.assign(total2d, 0.0f);
        _phi2_new.assign(total2d, 0.0f);
        precursor_flat.assign(total2d * 6, 0.0f);

        // XS SoA
        xs_D1.assign(total2d,0); xs_D2.assign(total2d,0);
        xs_SigR1.assign(total2d,0); xs_SigR2.assign(total2d,0);
        xs_SigS12.assign(total2d,0);
        xs_nuSF1.assign(total2d,0); xs_nuSF2.assign(total2d,0);
        xs_chi1.assign(total2d,0);  xs_chi2.assign(total2d,0);

        // Profil initial cosinus — remplit toutes les sub_xy×sub_xy sous-cellules
        float R = std::sqrt(grid.offsetX*grid.offsetX + grid.offsetZ*grid.offsetZ);
        if (R < 0.1f) R = 1.0f;
        for (const auto& cu : grid.cubes) {
            float r = std::sqrt(cu.pos.x*cu.pos.x + cu.pos.z*cu.pos.z);
            float v = cosf(std::min((float)M_PI*r/(2.0f*R*1.05f), (float)M_PI*0.499f));
            // Remplir les sub_xy×sub_xy sous-cellules de cet assemblage
            for (int dr=0; dr<sub_xy; ++dr)
            for (int dc=0; dc<sub_xy; ++dc) {
                int pr = cu.row     * sub_xy + dr;
                int pc = cu.col_idx * sub_xy + dc;
                int i  = pr * grid_cols + pc;
                if (i < 0 || i >= total2d) continue;
                phi1_flat[i] = v * 0.1f;
                phi2_flat[i] = v * 0.9f;
                zone_flat[i] = 1;
            }
        }

        // Zones — remplit les sub_xy×sub_xy sous-cellules pour chaque zone
        _moderatorNodes.clear(); _reflectorNodes.clear(); _controlRods.clear();
        for (const auto& zn : grid.zoneNodes) {
            for (int dr=0; dr<sub_xy; ++dr)
            for (int dc=0; dc<sub_xy; ++dc) {
                int pr  = zn.row * sub_xy + dr;
                int pc  = zn.col * sub_xy + dc;
                int i2d = pr * grid_cols + pc;
                if (i2d<0 || i2d>=total2d) continue;
                if (zn.zone==NodeZone::MODERATOR) {
                    _moderatorNodes.push_back({i2d, zn.param});
                    zone_flat[i2d] = 2;
                } else if (zn.zone==NodeZone::REFLECTOR) {
                    _reflectorNodes.push_back(i2d);
                    zone_flat[i2d] = 3;
                } else if (zn.zone==NodeZone::CONTROL_ROD) {
                    _controlRods.push_back({zn.row, zn.col, zn.param});
                    zone_flat[i2d] = 4;
                }
            } // dc
        } // zn

        // Calcul XS initial
        rebuildXS(grid, T0, 1.0f);

        // Précurseurs à l'équilibre
        _initPrecursorsEquilibrium();

        // ── Warmup CPU : 20 itérations pour converger vers le mode fondamental
        {
            const auto& pg2  = PrecursorData::get(_rt);
            float beta_tot2  = PrecursorData::beta_total(_rt);
            for (int iter=0; iter<20; ++iter) {
                float Fold = _computeF_CPU();
                if (Fold < 1e-30f) Fold = 1e-30f;
                _stepCPU(pg2, beta_tot2);
                float Fnew = _computeF_CPU();
                if (Fnew < 1e-30f) Fnew = 1e-30f;
                params.k_eff *= Fnew / Fold;
                params.k_eff  = std::max(0.5f, std::min(params.k_eff, 2.5f));
                float sc = Fold / Fnew;
                for (int i=0;i<total2d;++i) {
                    phi1_flat[i] *= sc;
                    phi2_flat[i] *= sc;
                }
            }
            k_eff = params.k_eff;
            std::cout << "[NeutronCompute v2] Warmup k_eff=" << k_eff << "\n";
        }

        // F0 sur le mode convergé (référence puissance nominale)
        _F0 = _computeF_CPU();
        if (_F0 < 1e-30f) _F0 = 1.0f;

        // ── Xénon/Iode/Samarium ──────────────────────────────
        float phi2_max = *std::max_element(phi2_flat.begin(), phi2_flat.end());
        xenon.init(total2d, std::max(phi2_max, 1e-10f));
        std::vector<float> Fsrc(total2d);
        for (int i=0;i<total2d;++i)
            Fsrc[i] = xs_nuSF1[i]*phi1_flat[i] + xs_nuSF2[i]*phi2_flat[i];
        xenon.setEquilibrium(Fsrc, phi2_flat);
        xenonActive = true;

        // ── Init GPU — cleanup d'abord si déjà initialisé ────
        try {
            if (gpuAccel) {          // réinit : détruire les anciens buffers
                _cleanupGPU();
                gpuAccel = false;
                _pingPong = false;
            }
            _initGPU();
            gpuAccel = true;
            std::cout << "[NeutronCompute v2] GPU SoA actif — "
                      << total2d << " cellules\n";
        } catch (const std::exception& e) {
            gpuAccel = false;
            std::cout << "[NeutronCompute v2] Fallback CPU : " << e.what() << "\n";
        }

        ready = true;
        return true;
    }

    // ── rebuildXS ───────────────────────────────────────────
    // T0       : T caloporteur entrée (°C)
    // rho_mod  : densité relative modérateur (1.0 = nominal)
    // Utilise T_mod_avg et C_bore_ppm (membres, mis à jour par main)
    void rebuildXS(const GridData& grid, float T0, float rho_mod) {
        for (int i=0; i<total2d; ++i) {
            int row=i/grid_cols, col=i%grid_cols;
            // Retrouver l'assemblage parent (espace assemblage)
            int assy_row = row / sub_xy;
            int assy_col = col / sub_xy;
            float T_fuel = T0;
            for (const auto& c : grid.cubes)
                if (c.row==assy_row && c.col_idx==assy_col) { T_fuel=c.temperature; break; }

            int zt = zone_flat[i];
            XS2G xs;
            if      (zt==1) xs = NeutronCrossSection::fuelFull(
                                    _rt, _epsilon, T_fuel,
                                    rho_mod, T_mod_avg, C_bore_ppm);
            else if (zt==2) xs = NeutronCrossSection::moderator(_rt, rho_mod);
            else if (zt==3) xs = NeutronCrossSection::reflector(_rt);
            else if (zt==4) {
                float ins=1.0f;
                for (const auto& cr:_controlRods)
                    if (cr.row==row&&cr.col==col) { ins=cr.insertFraction; break; }
                xs = NeutronCrossSection::controlRod(ins);
            }

            // Écriture SoA
            xs_D1[i]=xs.D[0];      xs_D2[i]=xs.D[1];
            xs_SigR1[i]=xs.SigR[0]; xs_SigR2[i]=xs.SigR[1];
            xs_SigS12[i]=xs.SigS12;
            xs_nuSF1[i]=xs.nuSigF[0]; xs_nuSF2[i]=xs.nuSigF[1];
            xs_chi1[i]=xs.chi[0];   xs_chi2[i]=xs.chi[1];
        }

        // ── Appliquer empoisonnement Xénon/Samarium à SigR2 ──
        if (xenonActive)
            xenon.applyToSigR2(xs_SigR2);

        if (gpuAccel) _uploadXS_SoA();
    }

    // ── step ────────────────────────────────────────────────
    void step(int nDiff=3, int nPower=3) {
        if (!ready) return;

        const auto& pg    = PrecursorData::get(_rt);
        float beta_tot    = PrecursorData::beta_total(_rt);

        for (int pw=0; pw<nPower; ++pw) {
            // Upload params avec k_eff courant
            if (gpuAccel) _uploadParams();

            // F_old : calculé AVANT le pas (sur phi courant)
            float F_old = _computeF_CPU();
            if (F_old < 1e-30f) F_old = 1e-30f;

            // Pas de diffusion
            for (int d=0; d<nDiff; ++d) {
                if (gpuAccel) _stepGPU();
                else          _stepCPU(pg, beta_tot);
            }

            // Readback GPU → CPU AVANT de calculer F_new
            if (gpuAccel) _readbackGPU();

            // F_new : calculé APRÈS le pas (sur phi mis à jour)
            float F_new = _computeF_CPU();
            if (F_new < 1e-30f) F_new = 1e-30f;

            // Mise à jour k_eff : méthode itération de puissance
            params.k_eff *= F_new / F_old;
            params.k_eff  = std::max(0.5f, std::min(params.k_eff, 2.5f));
            k_eff = params.k_eff;

            // Normalisation du flux pour éviter la divergence/extinction
            // On renormalise phi pour que F revienne à F_old (physique stable)
            float scale = (F_new > 1e-30f) ? F_old / F_new : 1.0f;
            for (int i=0;i<total2d;++i) {
                phi1_flat[i] *= scale;
                phi2_flat[i] *= scale;
            }
            if (gpuAccel) {
                // Ré-upload les phi normalisés sur le GPU
                VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(float);
                VkBuffer dst1 = _pingPong ? _bPhi1B : _bPhi1A;
                VkBuffer dst2 = _pingPong ? _bPhi2B : _bPhi2A;
                _ctx->uploadToDeviceLocal(dst1, phi1_flat.data(), sz);
                _ctx->uploadToDeviceLocal(dst2, phi2_flat.data(), sz);
            }
        }

        // ── Mise à jour Xénon ─────────────────────────────────
        if (xenonActive) {
            std::vector<float> Fsrc(total2d);
            for (int i=0;i<total2d;++i)
                Fsrc[i] = xs_nuSF1[i]*phi1_flat[i] + xs_nuSF2[i]*phi2_flat[i];
            xenon.step(params.dt, Fsrc, phi2_flat);
        }

        // ── phi_total, réactivité, puissance ─────────────────
        float F_cur = _computeF_CPU();
        for (int i=0;i<total2d;++i)
            phi_total[i] = phi1_flat[i] + phi2_flat[i];

        reactivity = (k_eff - 1.0f) / k_eff;
        // Puissance relative : F_cur / F0 (F0 fixé à l'init)
        power_rel  = (_F0 > 1e-30f) ? (F_cur / _F0) : 1.0f;
        // Clamp pour éviter NaN à l'affichage
        if (!std::isfinite(power_rel) || power_rel < 0.0f) power_rel = 0.0f;
        if (power_rel > 1000.0f) power_rel = 1000.0f;
    }

    // ── applyToGrid ─────────────────────────────────────────
    void applyToGrid(GridData& grid) {
        float phi_max = *std::max_element(phi_total.begin(), phi_total.end());
        if (phi_max < 1e-10f) phi_max = 1.0f;
        for (auto& cu : grid.cubes) {
            // Moyenne du flux sur les sub_xy×sub_xy sous-cellules
            float phi_avg = 0.0f; int cnt = 0;
            for (int dr=0; dr<sub_xy; ++dr)
            for (int dc=0; dc<sub_xy; ++dc) {
                int pr = cu.row     * sub_xy + dr;
                int pc = cu.col_idx * sub_xy + dc;
                int i  = pr * grid_cols + pc;
                if (i<0||i>=total2d) continue;
                phi_avg += phi_total[i]; cnt++;
            }
            cu.flux = (cnt>0) ? (phi_avg/cnt) / phi_max : 0.0f;
        }
    }

    // ── writeXS (pour compatibilité) ────────────────────────
    void writeXS(int i, const XS2G& xs) {
        xs_D1[i]=xs.D[0];   xs_D2[i]=xs.D[1];
        xs_SigR1[i]=xs.SigR[0]; xs_SigR2[i]=xs.SigR[1];
        xs_SigS12[i]=xs.SigS12;
        xs_nuSF1[i]=xs.nuSigF[0]; xs_nuSF2[i]=xs.nuSigF[1];
        xs_chi1[i]=xs.chi[0];  xs_chi2[i]=xs.chi[1];
    }

    // ── Nettoyage ────────────────────────────────────────────
    ~NeutronCompute() { if (gpuAccel) _cleanupGPU(); }

private:
    VulkanContext* _ctx = nullptr;
    ReactorType    _rt  = ReactorType::REP;
    float          _epsilon = 0.035f;
    float          _F0 = 1.0f;
    bool           _pingPong = false;

    std::vector<float> _phi1_new, _phi2_new;

    // ────────────────────────────────────────────────────────
    //  CPU FALLBACK
    // ────────────────────────────────────────────────────────
    void _stepCPU(const PrecursorGroup* pg, float beta_tot) {
        int NC=grid_cols, NR=grid_rows;
        float dt=params.dt, k=params.k_eff;
        float v1=params.v1, v2=params.v2;
        float idx2x = 1.0f/(params.dx*params.dx);
        float idx2z = 1.0f/(params.dz*params.dz);

        for (int row=0; row<NR; ++row) {
            for (int col=0; col<NC; ++col) {
                int i = row*NC+col;
                if (zone_flat[i]==0) { _phi1_new[i]=0; _phi2_new[i]=0; continue; }

                float D1=xs_D1[i], D2=xs_D2[i];
                float SR1=xs_SigR1[i], SR2=xs_SigR2[i];
                float SS12=xs_SigS12[i];
                float nSF1=xs_nuSF1[i], nSF2=xs_nuSF2[i];
                float c1=xs_chi1[i], c2=xs_chi2[i];
                float p1=phi1_flat[i], p2=phi2_flat[i];

                float Fsrc = nSF1*p1 + nSF2*p2;

                // Précurseurs
                int NG=6;
                float* C = &precursor_flat[i*NG];
                float delayed_src = 0.0f;
                for (int g=0; g<NG; ++g) {
                    float C_new=(C[g]+dt*(pg[g].beta/k)*Fsrc)/(1.0f+dt*pg[g].lambda);
                    C_new=std::max(0.0f,C_new);
                    delayed_src += pg[g].lambda*C_new;
                    C[g]=C_new;
                }

                // Laplacien FVM hétérogène
                auto gphi=[&](int r2,int c2,int g)->float{
                    if(r2<0||r2>=NR||c2<0||c2>=NC) return 0.0f;
                    int j=r2*NC+c2; if(zone_flat[j]==0) return 0.0f;
                    return (g==0)?phi1_flat[j]:phi2_flat[j];
                };
                auto gD=[&](int r2,int c2,int g)->float{
                    if(r2<0||r2>=NR||c2<0||c2>=NC) return 0.0f;
                    int j=r2*NC+c2;
                    return (g==0)?xs_D1[j]:xs_D2[j];
                };
                auto harm=[](float a,float b)->float{
                    return (a+b>1e-8f)?2.0f*a*b/(a+b):0.0f;
                };

                float lap1=0.0f, lap2=0.0f;
                int dr[]={0,0,-1,1}, dc[]={1,-1,0,0};
                // Anisotropie dx≠dz
                float idx[4]={idx2x,idx2x,idx2z,idx2z};
                for (int d=0;d<4;++d) {
                    float De1=harm(D1,gD(row+dr[d],col+dc[d],0));
                    lap1 += idx[d]*De1*(gphi(row+dr[d],col+dc[d],0)-p1);
                    float De2=harm(D2,gD(row+dr[d],col+dc[d],1));
                    lap2 += idx[d]*De2*(gphi(row+dr[d],col+dc[d],1)-p2);
                }

                float prompt_frac=(1.0f-beta_tot)/k;
                float src1=c1*(prompt_frac*Fsrc+delayed_src);
                float src2=c2*(prompt_frac*Fsrc+delayed_src)+SS12*p1;

                _phi1_new[i]=std::max(0.0f,(p1+dt*v1*(lap1+src1))/(1.0f+dt*v1*SR1));
                _phi2_new[i]=std::max(0.0f,(p2+dt*v2*(lap2+src2))/(1.0f+dt*v2*SR2));
            }
        }
        std::swap(phi1_flat,_phi1_new);
        std::swap(phi2_flat,_phi2_new);
    }

    float _computeF_CPU() {
        float F=0.0f;
        for (int i=0;i<total2d;++i)
            F += xs_nuSF1[i]*phi1_flat[i] + xs_nuSF2[i]*phi2_flat[i];
        return F;
    }
    float _computeF_CPU_fromFlat() { return _computeF_CPU(); }

    void _initPrecursorsEquilibrium() {
        const auto& pg = PrecursorData::get(_rt);
        float F = _computeF_CPU();
        float k = params.k_eff;
        for (int i=0;i<total2d;++i) {
            if (zone_flat[i]==0) continue;
            float Fsrc = xs_nuSF1[i]*phi1_flat[i] + xs_nuSF2[i]*phi2_flat[i];
            for (int g=0;g<6;++g)
                precursor_flat[i*6+g] = (pg[g].beta/k)*Fsrc/pg[g].lambda;
        }
    }

    // ────────────────────────────────────────────────────────
    //  GPU VULKAN — SoA buffers + neutron_fvm.comp
    // ────────────────────────────────────────────────────────

    // ── Buffers phi (ping-pong DEVICE_LOCAL) ─────────────────
    VkBuffer       _bPhi1A=VK_NULL_HANDLE, _bPhi1B=VK_NULL_HANDLE;
    VkDeviceMemory _mPhi1A=VK_NULL_HANDLE, _mPhi1B=VK_NULL_HANDLE;
    VkBuffer       _bPhi2A=VK_NULL_HANDLE, _bPhi2B=VK_NULL_HANDLE;
    VkDeviceMemory _mPhi2A=VK_NULL_HANDLE, _mPhi2B=VK_NULL_HANDLE;

    // ── Précurseurs ping-pong (DEVICE_LOCAL) ──────────────────
    VkBuffer       _bPrecA=VK_NULL_HANDLE, _bPrecB=VK_NULL_HANDLE;
    VkDeviceMemory _mPrecA=VK_NULL_HANDLE, _mPrecB=VK_NULL_HANDLE;

    // ── XS SoA (HOST_VISIBLE|COHERENT — upload chaque frame) ──
    VkBuffer       _bD1=VK_NULL_HANDLE;
    VkDeviceMemory _mD1=VK_NULL_HANDLE;
    VkBuffer       _bD2=VK_NULL_HANDLE;       VkDeviceMemory _mD2=VK_NULL_HANDLE;
    VkBuffer       _bSR1=VK_NULL_HANDLE;      VkDeviceMemory _mSR1=VK_NULL_HANDLE;
    VkBuffer       _bSR2=VK_NULL_HANDLE;      VkDeviceMemory _mSR2=VK_NULL_HANDLE;
    VkBuffer       _bSS12=VK_NULL_HANDLE;     VkDeviceMemory _mSS12=VK_NULL_HANDLE;
    VkBuffer       _bnSF1=VK_NULL_HANDLE;     VkDeviceMemory _mnSF1=VK_NULL_HANDLE;
    VkBuffer       _bnSF2=VK_NULL_HANDLE;     VkDeviceMemory _mnSF2=VK_NULL_HANDLE;
    VkBuffer       _bchi1=VK_NULL_HANDLE;     VkDeviceMemory _mchi1=VK_NULL_HANDLE;
    VkBuffer       _bchi2=VK_NULL_HANDLE;     VkDeviceMemory _mchi2=VK_NULL_HANDLE;

    // ── Zone + params ──────────────────────────────────────────
    VkBuffer       _bZone=VK_NULL_HANDLE;     VkDeviceMemory _mZone=VK_NULL_HANDLE;
    VkBuffer       _bParams=VK_NULL_HANDLE;   VkDeviceMemory _mParams=VK_NULL_HANDLE;

    // ── Readback phi ───────────────────────────────────────────
    VkBuffer       _bRBphi1=VK_NULL_HANDLE;   VkDeviceMemory _mRBphi1=VK_NULL_HANDLE;
    VkBuffer       _bRBphi2=VK_NULL_HANDLE;   VkDeviceMemory _mRBphi2=VK_NULL_HANDLE;

    // ── Pipeline A — neutron_fvm.comp ─────────────────────────
    VkShaderModule        _shFVM=VK_NULL_HANDLE;
    VkDescriptorSetLayout _dslFVM=VK_NULL_HANDLE;
    VkPipelineLayout      _layoutFVM=VK_NULL_HANDLE;
    VkPipeline            _pipeFVM=VK_NULL_HANDLE;
    VkDescriptorSet       _descAB=VK_NULL_HANDLE;   // phi1A→phi1B
    VkDescriptorSet       _descBA=VK_NULL_HANDLE;   // phi1B→phi1A

    // ── Pipeline B — neutron_reduce.comp ──────────────────────
    int            _nWorkgroups=1;
    VkBuffer       _bPartial=VK_NULL_HANDLE;  VkDeviceMemory _mPartial=VK_NULL_HANDLE;
    VkBuffer       _bRedParams=VK_NULL_HANDLE; VkDeviceMemory _mRedParams=VK_NULL_HANDLE;
    VkBuffer       _bRBreduce=VK_NULL_HANDLE; VkDeviceMemory _mRBreduce=VK_NULL_HANDLE;

    VkShaderModule        _shRed=VK_NULL_HANDLE;
    VkDescriptorSetLayout _dslRed=VK_NULL_HANDLE;
    VkPipelineLayout      _layoutRed=VK_NULL_HANDLE;
    VkPipeline            _pipeRed=VK_NULL_HANDLE;
    VkDescriptorSet       _descRedA=VK_NULL_HANDLE;
    VkDescriptorSet       _descRedB=VK_NULL_HANDLE;

    // ── _initGPU ──────────────────────────────────────────────
    void _initGPU() {
        VkDevice dev = _ctx->device;
        VkDeviceSize sz  = (VkDeviceSize)total2d * sizeof(float);
        VkDeviceSize szP = (VkDeviceSize)total2d * 6 * sizeof(float);  // précurseurs
        VkDeviceSize szZ = (VkDeviceSize)total2d * sizeof(int);

        _nWorkgroups = (total2d + 255) / 256;
        VkDeviceSize szPart = (VkDeviceSize)_nWorkgroups * sizeof(float);

        using VCB = VulkanContext;
        constexpr auto DEV  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        constexpr auto HOST = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        constexpr auto STO  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        constexpr auto UNI  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        constexpr auto TRA  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Phi ping-pong DEVICE_LOCAL
        _ctx->createBuffer(sz, STO|TRA, DEV, _bPhi1A, _mPhi1A);
        _ctx->createBuffer(sz, STO|TRA, DEV, _bPhi1B, _mPhi1B);
        _ctx->createBuffer(sz, STO|TRA, DEV, _bPhi2A, _mPhi2A);
        _ctx->createBuffer(sz, STO|TRA, DEV, _bPhi2B, _mPhi2B);

        // Précurseurs ping-pong DEVICE_LOCAL
        _ctx->createBuffer(szP, STO|TRA, DEV, _bPrecA, _mPrecA);
        _ctx->createBuffer(szP, STO|TRA, DEV, _bPrecB, _mPrecB);

        // XS SoA HOST_VISIBLE (usage=STO uniquement, flags mémoire=HOST)
        _ctx->createBuffer(sz, STO, HOST, _bD1,   _mD1);
        _ctx->createBuffer(sz, STO, HOST, _bD2,   _mD2);
        _ctx->createBuffer(sz, STO, HOST, _bSR1,  _mSR1);
        _ctx->createBuffer(sz, STO, HOST, _bSR2,  _mSR2);
        _ctx->createBuffer(sz, STO, HOST, _bSS12, _mSS12);
        _ctx->createBuffer(sz, STO, HOST, _bnSF1, _mnSF1);
        _ctx->createBuffer(sz, STO, HOST, _bnSF2, _mnSF2);
        _ctx->createBuffer(sz, STO, HOST, _bchi1, _mchi1);
        _ctx->createBuffer(sz, STO, HOST, _bchi2, _mchi2);

        // Zone HOST_VISIBLE
        _ctx->createBuffer(szZ, STO, HOST, _bZone, _mZone);

        // Params uniform HOST_VISIBLE
        _ctx->createBuffer(sizeof(NeutronParams), UNI, HOST,
                           _bParams, _mParams);

        // Readback phi HOST_VISIBLE (TRA_DST pour recevoir la copie GPU→CPU)
        _ctx->createBuffer(sz, TRA|VK_BUFFER_USAGE_TRANSFER_DST_BIT, HOST,
                           _bRBphi1, _mRBphi1);
        _ctx->createBuffer(sz, TRA|VK_BUFFER_USAGE_TRANSFER_DST_BIT, HOST,
                           _bRBphi2, _mRBphi2);

        // Reduce
        _ctx->createBuffer(szPart, STO|TRA, DEV, _bPartial, _mPartial);
        _ctx->createBuffer(sizeof(ReduceParams), UNI, HOST,
                           _bRedParams, _mRedParams);
        _ctx->createBuffer(szPart, TRA|VK_BUFFER_USAGE_TRANSFER_DST_BIT, HOST,
                           _bRBreduce, _mRBreduce);

        // Upload initial
        _uploadXS_SoA();
        _uploadZone();
        _uploadPhi();
        _uploadPrecursors();
        _uploadParams();
        _uploadReduceParams();

        // Pipelines
        _createFVMPipeline();
        _createRedPipeline();
        _writeFVMDesc();
        _writeRedDesc();
    }

    // ── Upload SoA XS — 9 appels map/memcpy/unmap ─────────────
    void _uploadXS_SoA() {
        struct { VkDeviceMemory mem; const float* data; } bufs[] = {
            {_mD1,   xs_D1.data()},   {_mD2,   xs_D2.data()},
            {_mSR1,  xs_SigR1.data()},{_mSR2,  xs_SigR2.data()},
            {_mSS12, xs_SigS12.data()},{_mnSF1, xs_nuSF1.data()},
            {_mnSF2, xs_nuSF2.data()},{_mchi1, xs_chi1.data()},
            {_mchi2, xs_chi2.data()},
        };
        VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(float);
        for (auto& b : bufs) {
            void* ptr;
            vkMapMemory(_ctx->device, b.mem, 0, VK_WHOLE_SIZE, 0, &ptr);
            memcpy(ptr, b.data, sz);
            vkUnmapMemory(_ctx->device, b.mem);
        }
    }

    void _uploadZone() {
        VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(int);
        void* ptr;
        vkMapMemory(_ctx->device, _mZone, 0, VK_WHOLE_SIZE, 0, &ptr);
        memcpy(ptr, zone_flat.data(), sz);
        vkUnmapMemory(_ctx->device, _mZone);
    }

    void _uploadParams() {
        void* ptr;
        vkMapMemory(_ctx->device, _mParams, 0, VK_WHOLE_SIZE, 0, &ptr);
        memcpy(ptr, &params, sizeof(NeutronParams));
        vkUnmapMemory(_ctx->device, _mParams);
    }

    void _uploadReduceParams() {
        ReduceParams rp{total2d, {0,0,0}};
        void* ptr;
        vkMapMemory(_ctx->device, _mRedParams, 0, VK_WHOLE_SIZE, 0, &ptr);
        memcpy(ptr, &rp, sizeof(ReduceParams));
        vkUnmapMemory(_ctx->device, _mRedParams);
    }

    // Upload phi CPU → GPU (via staging car DEVICE_LOCAL)
    void _uploadPhi() {
        VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(float);
        _ctx->uploadToDeviceLocal(_bPhi1A, phi1_flat.data(), sz);
        _ctx->uploadToDeviceLocal(_bPhi2A, phi2_flat.data(), sz);
        // phi1B/phi2B initialisés à 0
        std::vector<float> zeros(total2d, 0.0f);
        _ctx->uploadToDeviceLocal(_bPhi1B, zeros.data(), sz);
        _ctx->uploadToDeviceLocal(_bPhi2B, zeros.data(), sz);
    }

    void _uploadPrecursors() {
        VkDeviceSize sz = (VkDeviceSize)total2d * 6 * sizeof(float);
        _ctx->uploadToDeviceLocal(_bPrecA, precursor_flat.data(), sz);
        std::vector<float> zeros(total2d*6, 0.0f);
        _ctx->uploadToDeviceLocal(_bPrecB, zeros.data(), sz);
    }

    // ── Pipeline A : neutron_fvm.comp (17 bindings) ───────────
    void _createFVMPipeline() {
        VkDevice dev = _ctx->device;

        // DSL : 17 bindings (0-15 STORAGE, 16 UNIFORM)
        std::vector<VkDescriptorSetLayoutBinding> bindings(17);
        for (uint32_t i=0; i<16; i++) {
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        }
        bindings[16] = {16, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

        VkDescriptorSetLayoutCreateInfo dslCI{};
        dslCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslCI.bindingCount = (uint32_t)bindings.size();
        dslCI.pBindings    = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &dslCI, nullptr, &_dslFVM));

        VkPipelineLayoutCreateInfo plCI{};
        plCI.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.setLayoutCount = 1;
        plCI.pSetLayouts    = &_dslFVM;
        VK_CHECK(vkCreatePipelineLayout(dev, &plCI, nullptr, &_layoutFVM));

        _pipeFVM = _ctx->createComputePipeline(
            "compute/shaders/neutron_fvm.spv", _layoutFVM, _shFVM);
    }

    // ── Pipeline B : neutron_reduce.comp (7 bindings) ─────────
    void _createRedPipeline() {
        VkDevice dev = _ctx->device;

        std::vector<VkDescriptorSetLayoutBinding> bindings(7);
        for (uint32_t i=0;i<6;i++)
            bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        bindings[6]={6,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};

        VkDescriptorSetLayoutCreateInfo dslCI{};
        dslCI.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslCI.bindingCount=7; dslCI.pBindings=bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(dev,&dslCI,nullptr,&_dslRed));

        VkPipelineLayoutCreateInfo plCI{};
        plCI.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.setLayoutCount=1; plCI.pSetLayouts=&_dslRed;
        VK_CHECK(vkCreatePipelineLayout(dev,&plCI,nullptr,&_layoutRed));

        _pipeRed = _ctx->createComputePipeline(
            "compute/shaders/neutron_reduce.spv", _layoutRed, _shRed);
    }

    // ── Écriture descripteurs FVM ─────────────────────────────
    void _writeFVMDesc() {
        VkDevice dev = _ctx->device;
        VkDescriptorSetLayout layouts[2] = {_dslFVM, _dslFVM};

        VkDescriptorSetAllocateInfo ai{};
        ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool=_ctx->descriptorPool;
        ai.descriptorSetCount=2; ai.pSetLayouts=layouts;
        VkDescriptorSet sets[2];
        VK_CHECK(vkAllocateDescriptorSets(dev,&ai,sets));
        _descAB=sets[0]; _descBA=sets[1];

        // AB : lit phi1A/phi2A/precA, écrit phi1B/phi2B/precB
        _writeFVMDescSet(_descAB,
            _bPhi1A,_bPhi2A, _bPhi1B,_bPhi2B,
            _bPrecA,_bPrecB);
        // BA : lit phi1B/phi2B/precB, écrit phi1A/phi2A/precA
        _writeFVMDescSet(_descBA,
            _bPhi1B,_bPhi2B, _bPhi1A,_bPhi2A,
            _bPrecB,_bPrecA);
    }

    void _writeFVMDescSet(VkDescriptorSet ds,
        VkBuffer phi1in, VkBuffer phi2in,
        VkBuffer phi1out,VkBuffer phi2out,
        VkBuffer precIn, VkBuffer precOut)
    {
        // Tableau de buffer infos : bindings 0-15 (storage) + 16 (uniform)
        VkDescriptorBufferInfo bi[17]{};
        auto setBuf = [&](int idx, VkBuffer buf, VkDeviceSize sz=VK_WHOLE_SIZE){
            bi[idx].buffer=buf; bi[idx].offset=0; bi[idx].range=sz;
        };
        setBuf(0,phi1in); setBuf(1,phi2in);
        setBuf(2,phi1out); setBuf(3,phi2out);
        setBuf(4,_bD1);  setBuf(5,_bD2);
        setBuf(6,_bSR1); setBuf(7,_bSR2);
        setBuf(8,_bSS12);
        setBuf(9,_bnSF1); setBuf(10,_bnSF2);
        setBuf(11,_bchi1); setBuf(12,_bchi2);
        setBuf(13,_bZone); setBuf(14,precIn); setBuf(15,precOut);
        setBuf(16,_bParams);

        std::vector<VkWriteDescriptorSet> writes(17);
        for (int i=0;i<17;i++) {
            writes[i]={};
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet=ds;
            writes[i].dstBinding=(uint32_t)i;
            writes[i].descriptorCount=1;
            writes[i].descriptorType=(i<16)
                ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[i].pBufferInfo=&bi[i];
        }
        vkUpdateDescriptorSets(_ctx->device, 17, writes.data(), 0, nullptr);
    }

    // ── Écriture descripteurs Reduce ──────────────────────────
    void _writeRedDesc() {
        VkDevice dev = _ctx->device;
        VkDescriptorSetLayout layouts[2] = {_dslRed, _dslRed};
        VkDescriptorSetAllocateInfo ai{};
        ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool=_ctx->descriptorPool;
        ai.descriptorSetCount=2; ai.pSetLayouts=layouts;
        VkDescriptorSet sets[2];
        VK_CHECK(vkAllocateDescriptorSets(dev,&ai,sets));
        _descRedA=sets[0]; _descRedB=sets[1];

        auto writeRed=[&](VkDescriptorSet ds, VkBuffer phi1, VkBuffer phi2) {
            VkDescriptorBufferInfo bi[7]{};
            bi[0]={phi1,0,VK_WHOLE_SIZE}; bi[1]={phi2,0,VK_WHOLE_SIZE};
            bi[2]={_bnSF1,0,VK_WHOLE_SIZE}; bi[3]={_bnSF2,0,VK_WHOLE_SIZE};
            bi[4]={_bZone,0,VK_WHOLE_SIZE};
            bi[5]={_bPartial,0,VK_WHOLE_SIZE};
            bi[6]={_bRedParams,0,VK_WHOLE_SIZE};

            VkWriteDescriptorSet ws[7]{};
            for (int i=0;i<7;i++){
                ws[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                ws[i].dstSet=ds; ws[i].dstBinding=(uint32_t)i;
                ws[i].descriptorCount=1;
                ws[i].descriptorType=(i<6)
                    ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                    : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                ws[i].pBufferInfo=&bi[i];
            }
            vkUpdateDescriptorSets(dev,7,ws,0,nullptr);
        };
        writeRed(_descRedA, _bPhi1A, _bPhi2A);
        writeRed(_descRedB, _bPhi1B, _bPhi2B);
    }

    // ── Dispatch diffusion ────────────────────────────────────
    void _stepGPU() {
        auto cb = _ctx->beginOneShot();

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeFVM);
        VkDescriptorSet ds = _pingPong ? _descBA : _descAB;
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _layoutFVM, 0, 1, &ds, 0, nullptr);

        uint32_t gx = ((uint32_t)grid_cols + 7) / 8;
        uint32_t gy = ((uint32_t)grid_rows + 7) / 8;
        vkCmdDispatch(cb, gx, gy, 1);

        VkMemoryBarrier bar{};
        bar.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        bar.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
        bar.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,1,&bar,0,nullptr,0,nullptr);

        _pingPong = !_pingPong;
        _ctx->endOneShot(cb);
    }

    // ── Réduction GPU → F ─────────────────────────────────────
    float _computeF_GPU() {
        auto cb = _ctx->beginOneShot();

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeRed);
        VkDescriptorSet ds = _pingPong ? _descRedB : _descRedA;
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _layoutRed, 0, 1, &ds, 0, nullptr);
        vkCmdDispatch(cb, (uint32_t)_nWorkgroups, 1, 1);

        VkMemoryBarrier bar{};
        bar.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        bar.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
        bar.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,1,&bar,0,nullptr,0,nullptr);

        VkBufferCopy reg{0,0,(VkDeviceSize)_nWorkgroups*sizeof(float)};
        vkCmdCopyBuffer(cb, _bPartial, _bRBreduce, 1, &reg);
        _ctx->endOneShot(cb);

        void* ptr;
        vkMapMemory(_ctx->device, _mRBreduce, 0,
                    VK_WHOLE_SIZE, 0, &ptr);
        float F=0.0f;
        for (int i=0;i<_nWorkgroups;i++) F+=((float*)ptr)[i];
        vkUnmapMemory(_ctx->device, _mRBreduce);
        return F;
    }

    // ── Readback phi GPU → CPU ────────────────────────────────
    void _readbackGPU() {
        VkDeviceSize sz = (VkDeviceSize)total2d * sizeof(float);
        VkBuffer srcPhi1 = _pingPong ? _bPhi1B : _bPhi1A;
        VkBuffer srcPhi2 = _pingPong ? _bPhi2B : _bPhi2A;

        auto cb = _ctx->beginOneShot();
        VkBufferCopy reg{0,0,sz};
        vkCmdCopyBuffer(cb, srcPhi1, _bRBphi1, 1, &reg);
        vkCmdCopyBuffer(cb, srcPhi2, _bRBphi2, 1, &reg);
        _ctx->endOneShot(cb);

        void* ptr1; void* ptr2;
        vkMapMemory(_ctx->device, _mRBphi1, 0, VK_WHOLE_SIZE, 0, &ptr1);
        vkMapMemory(_ctx->device, _mRBphi2, 0, VK_WHOLE_SIZE, 0, &ptr2);
        memcpy(phi1_flat.data(), ptr1, sz);
        memcpy(phi2_flat.data(), ptr2, sz);
        vkUnmapMemory(_ctx->device, _mRBphi1);
        vkUnmapMemory(_ctx->device, _mRBphi2);
    }

    // ── Cleanup ───────────────────────────────────────────────
    void _cleanupGPU() {
        if (!_ctx) return;
        vkDeviceWaitIdle(_ctx->device);
        VkDevice dev = _ctx->device;

        auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m){
            if(b){vkDestroyBuffer(dev,b,nullptr);b=VK_NULL_HANDLE;}
            if(m){vkFreeMemory(dev,m,nullptr);m=VK_NULL_HANDLE;}
        };

        destroyBuf(_bPhi1A,_mPhi1A); destroyBuf(_bPhi1B,_mPhi1B);
        destroyBuf(_bPhi2A,_mPhi2A); destroyBuf(_bPhi2B,_mPhi2B);
        destroyBuf(_bPrecA,_mPrecA); destroyBuf(_bPrecB,_mPrecB);
        destroyBuf(_bD1,_mD1);   destroyBuf(_bD2,_mD2);
        destroyBuf(_bSR1,_mSR1); destroyBuf(_bSR2,_mSR2);
        destroyBuf(_bSS12,_mSS12);
        destroyBuf(_bnSF1,_mnSF1); destroyBuf(_bnSF2,_mnSF2);
        destroyBuf(_bchi1,_mchi1); destroyBuf(_bchi2,_mchi2);
        destroyBuf(_bZone,_mZone); destroyBuf(_bParams,_mParams);
        destroyBuf(_bRBphi1,_mRBphi1); destroyBuf(_bRBphi2,_mRBphi2);
        destroyBuf(_bPartial,_mPartial); destroyBuf(_bRedParams,_mRedParams);
        destroyBuf(_bRBreduce,_mRBreduce);

        if(_pipeFVM){vkDestroyPipeline(dev,_pipeFVM,nullptr);_pipeFVM=VK_NULL_HANDLE;}
        if(_pipeRed){vkDestroyPipeline(dev,_pipeRed,nullptr);_pipeRed=VK_NULL_HANDLE;}
        if(_layoutFVM){vkDestroyPipelineLayout(dev,_layoutFVM,nullptr);_layoutFVM=VK_NULL_HANDLE;}
        if(_layoutRed){vkDestroyPipelineLayout(dev,_layoutRed,nullptr);_layoutRed=VK_NULL_HANDLE;}
        if(_dslFVM){vkDestroyDescriptorSetLayout(dev,_dslFVM,nullptr);_dslFVM=VK_NULL_HANDLE;}
        if(_dslRed){vkDestroyDescriptorSetLayout(dev,_dslRed,nullptr);_dslRed=VK_NULL_HANDLE;}
        if(_shFVM){vkDestroyShaderModule(dev,_shFVM,nullptr);_shFVM=VK_NULL_HANDLE;}
        if(_shRed){vkDestroyShaderModule(dev,_shRed,nullptr);_shRed=VK_NULL_HANDLE;}

        // Libérer explicitement les descriptor sets (pool géré par VulkanContext)
        VkDescriptorSet setsToFree[4] = {_descAB, _descBA, _descRedA, _descRedB};
        int nFree = 0;
        for (auto s : setsToFree) if (s != VK_NULL_HANDLE) ++nFree;
        if (nFree > 0) {
            std::vector<VkDescriptorSet> valid;
            for (auto s : setsToFree) if (s != VK_NULL_HANDLE) valid.push_back(s);
            vkFreeDescriptorSets(dev, _ctx->descriptorPool,
                                 (uint32_t)valid.size(), valid.data());
        }
        _descAB=VK_NULL_HANDLE; _descBA=VK_NULL_HANDLE;
        _descRedA=VK_NULL_HANDLE; _descRedB=VK_NULL_HANDLE;

        _pingPong = false;
    }
};