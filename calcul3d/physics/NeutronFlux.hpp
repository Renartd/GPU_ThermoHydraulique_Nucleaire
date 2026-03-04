#pragma once
// ============================================================
//  NeutronFlux.hpp
//  Flux neutronique + puissance volumique q'''
// ============================================================
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include <algorithm>
#include "../core/GridData.hpp"

enum class FluxMode { DIFFUSION_2G, COSINUS_REP, UNIFORME };

class NeutronFlux {
public:
    float phi0        = 3.0e13f;   // neutrons/cm²/s
    float sigma_f     = 0.09f;     // cm⁻¹
    float E_fission   = 3.2e-11f;  // J
    float alpha_doppler = -3.0e-5f;
    float T_ref       = 300.0f;

    // Facteur d'échelle physique → valeur simulable
    // q''' réel ≈ 8.6e7 W/m³, mais avec dt=8318s → diverge
    // On normalise pour que ΔT_max ≈ 5°C/pas
    // ΔT = q''' × dt / rho_cp  ≤ 5°C
    // → q'''_max ≤ 5 × 2.578e6 / 8318 ≈ 1549 W/m³
    // On applique un facteur d'échelle automatique
    float scale = 1.0f; // calculé dans init()

    FluxMode mode = FluxMode::COSINUS_REP;

    // Initialise le facteur d'échelle selon dt et rho_cp
    void init(float dt, float rho_cp, float dT_per_step = 2.0f) {
        // q'''_max × dt / rho_cp = dT_per_step
        float q_max_raw = sigma_f * phi0 * E_fission * 1e6f;
        float q_max_target = dT_per_step * rho_cp / dt;
        scale = q_max_target / q_max_raw;
        std::cout << "[NeutronFlux] scale=" << scale
                  << "  q'''_max effectif=" << q_max_raw * scale << " W/m³\n";
    }

    std::vector<float> calculer(GridData& grid,
                                 const std::vector<float>& temperatures) {
        int N = (int)grid.cubes.size();
        std::vector<float> q_vol(N, 0.0f);

        float R = sqrtf(grid.offsetX*grid.offsetX + grid.offsetZ*grid.offsetZ);
        if (R < 0.001f) R = 1.0f;

        for (int i = 0; i < N; ++i) {
            const auto& cube = grid.cubes[i];
            float phi;
            if (mode == FluxMode::COSINUS_REP) {
                float r = sqrtf(cube.pos.x*cube.pos.x + cube.pos.z*cube.pos.z);
                float arg = (float)M_PI * r / (2.0f * R * 1.05f);
                phi = phi0 * cosf(fminf(arg, (float)M_PI * 0.499f));
            } else {
                phi = phi0;
            }

            // Rétroaction Doppler
            float T = (i < (int)temperatures.size()) ? temperatures[i] : T_ref;
            float doppler = 1.0f + alpha_doppler * (T - T_ref);
            doppler = fmaxf(doppler, 0.1f);
            phi *= doppler;

            grid.cubes[i].flux = phi / phi0;

            // q''' avec facteur d'échelle
            q_vol[i] = sigma_f * phi * E_fission * 1e6f * scale;
        }
        return q_vol;
    }

    std::string modeStr() const {
        return mode == FluxMode::COSINUS_REP ? "Cosinus REP" : "Uniforme";
    }
};