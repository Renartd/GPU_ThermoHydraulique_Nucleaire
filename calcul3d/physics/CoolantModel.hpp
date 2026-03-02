#pragma once
// ============================================================
//  CoolantModel.hpp — Modèle fluide caloporteur circuit ouvert
//  Supporte : Eau (REP), Sodium (RNR-Na), Plomb-Bi (RNR-Pb), Hélium (RHT)
// ============================================================
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include "../core/GridData.hpp"

// ---------------------------------------------------------------
//  Type de fluide
// ---------------------------------------------------------------
enum class FluidType { EAU, SODIUM, PLOMB_BI, HELIUM };

// ---------------------------------------------------------------
//  Mode de convection
// ---------------------------------------------------------------
enum class ConvectionMode { NATURELLE, FORCEE, COMBINEE };

// ---------------------------------------------------------------
//  Propriétés thermophysiques d'un fluide à T(°C) et P(bar)
// ---------------------------------------------------------------
struct FluidProps {
    float rho;    // densité (kg/m³)
    float cp;     // capacité calorifique (J/kg·K)
    float mu;     // viscosité dynamique (Pa·s)
    float k;      // conductivité thermique (W/m·K)
    float beta;   // coeff dilatation thermique (1/K)
    float Pr;     // Prandtl = mu*cp/k
};

// ---------------------------------------------------------------
//  État du fluide pour chaque cellule de la grille
// ---------------------------------------------------------------
struct CoolantCell {
    float T_in;       // température entrée cellule (°C)
    float T_out;      // température sortie cellule (°C)
    float velocity;   // vitesse ascendante (m/s)
    float massFlow;   // débit massique (kg/s)
    float Re;         // Reynolds
};

// ---------------------------------------------------------------
//  Paramètres du modèle caloporteur
// ---------------------------------------------------------------
struct CoolantParams {
    FluidType      fluid        = FluidType::EAU;
    ConvectionMode convMode     = ConvectionMode::FORCEE;  // défaut REP
    float          T_inlet      = 286.0f;   // °C — entrée bas
    float          P_bar        = 155.0f;   // bar
    float          dP_moteur    = 2.0f;     // bar — pression motrice pompe

    // Géométrie canal (espace entre assemblages)
    float D_h       = 0.01f;    // diamètre hydraulique canal (m)
    float A_canal   = 1e-4f;    // section du canal (m²)
    float H_coeur   = 4.00f;    // hauteur active (m)
};

// ---------------------------------------------------------------
//  CoolantModel
// ---------------------------------------------------------------
struct CoolantModel {

    CoolantParams params;
    // T_fluid[row][col] = température du fluide à cette position
    std::vector<std::vector<float>> T_fluid;
    // v_fluid[row][col] = vitesse locale
    std::vector<std::vector<float>> v_fluid;
    bool initialized = false;

    // ---- Propriétés fluide selon type, T et P ----
    static FluidProps getProps(FluidType ft, float T_C, float P_bar) {
        FluidProps p;
        switch (ft) {

        case FluidType::EAU:
            // Eau liquide sous pression (155 bar REP) — corrélations IAPWS simplifiées
            p.rho  = 1000.0f - 0.4f*(T_C - 20.0f) - 3e-4f*(T_C-20.0f)*(T_C-20.0f);
            p.rho  = fmaxf(p.rho, 600.0f);
            p.cp   = 4200.0f - 1.5f*(T_C - 20.0f);
            p.cp   = fmaxf(p.cp, 3800.0f);
            p.mu   = 1e-3f * expf(-0.02f*(T_C - 20.0f));
            p.mu   = fmaxf(p.mu, 8e-5f);
            p.k    = 0.58f + 1.5e-3f*(T_C - 20.0f) - 2e-6f*(T_C-20.0f)*(T_C-20.0f);
            p.beta = 2e-4f + 4e-6f*(T_C - 20.0f);
            p.Pr   = p.mu * p.cp / p.k;
            break;

        case FluidType::SODIUM:
            // Sodium liquide (RNR-Na) — corrélations ANL
            p.rho  = 950.0f - 0.23f*(T_C - 100.0f);
            p.rho  = fmaxf(p.rho, 700.0f);
            p.cp   = 1260.0f - 0.5f*(T_C - 100.0f);
            p.cp   = fmaxf(p.cp, 1000.0f);
            p.mu   = 4e-4f * expf(556.0f / (T_C + 273.0f));
            p.mu   = fmaxf(p.mu, 1.5e-4f);
            p.k    = 92.0f - 0.056f*(T_C - 100.0f);
            p.k    = fmaxf(p.k, 50.0f);
            p.beta = 2.5e-4f;
            p.Pr   = p.mu * p.cp / p.k;
            break;

        case FluidType::PLOMB_BI:
            // Plomb-Bismuth eutectique LBE (RNR-Pb) — OECD NEA corrélations
            p.rho  = 10520.0f - 1.36f*(T_C - 125.0f);
            p.rho  = fmaxf(p.rho, 9000.0f);
            p.cp   = 146.5f - 0.01f*(T_C - 125.0f);
            p.cp   = fmaxf(p.cp, 130.0f);
            p.mu   = 4.94e-4f * expf(754.1f / (T_C + 273.0f));
            p.mu   = fmaxf(p.mu, 1e-3f);
            p.k    = 3.61f + 1.517e-2f*(T_C - 125.0f);
            p.k    = fmaxf(p.k, 3.0f);
            p.beta = 1.2e-4f;
            p.Pr   = p.mu * p.cp / p.k;
            break;

        case FluidType::HELIUM:
            // Hélium gaz (RHT) — gaz parfait + corrélations NIST
            {
                float T_K = T_C + 273.15f;
                float P_Pa = P_bar * 1e5f;
                p.rho  = (P_Pa * 4.003e-3f) / (8.314f * T_K);  // gaz parfait He
                p.cp   = 5193.0f;   // quasiment constant pour He
                p.mu   = 1.99e-5f * powf(T_K / 300.0f, 0.67f);
                p.k    = 0.152f * powf(T_K / 300.0f, 0.67f);
                p.beta = 1.0f / T_K;  // gaz parfait
                p.Pr   = p.mu * p.cp / p.k;
            }
            break;
        }
        return p;
    }

    // ---- Nom du fluide pour affichage ----
    static const char* fluidName(FluidType ft) {
        switch(ft) {
        case FluidType::EAU:      return "Eau H2O (REP)";
        case FluidType::SODIUM:   return "Sodium Na (RNR)";
        case FluidType::PLOMB_BI: return "Plomb-Bi LBE (RNR)";
        case FluidType::HELIUM:   return "Helium He (RHT)";
        }
        return "?";
    }

    // ---- Initialisation ----
    void init(const GridData& grid, const CoolantParams& p) {
        params = p;
        T_fluid.assign(grid.rows, std::vector<float>(grid.cols, p.T_inlet));
        v_fluid.assign(grid.rows, std::vector<float>(grid.cols, 0.0f));
        initialized = true;

        // Mode convection par défaut selon fluide
        if (params.convMode == ConvectionMode::FORCEE) {
            // Déjà bon
        }
        std::cout << "[Coolant] Init : " << fluidName(params.fluid)
                  << "  T_in=" << p.T_inlet << "°C  P=" << p.P_bar << " bar\n";
    }

    // ---- Calcul vitesse ----
    // v_forcee : Darcy-Weisbach simplifié   ΔP = f·(H/Dh)·ρv²/2
    // v_naturelle : thermosiphon            v = sqrt(2·g·β·ΔT·H)
    float calcVitesse(const FluidProps& fp, float dT_colonne, ConvectionMode mode) {
        const float g = 9.81f;
        float v = 0.0f;

        if (mode == ConvectionMode::FORCEE || mode == ConvectionMode::COMBINEE) {
            // Darcy-Weisbach : ΔP_moteur = f*(H/Dh)*(ρ*v²/2)
            // f ≈ 0.02 (turbulent lisse)
            float f = 0.02f;
            float dP_Pa = params.dP_moteur * 1e5f;
            float v2 = (2.0f * dP_Pa) / (fp.rho * f * (params.H_coeur / params.D_h));
            v += sqrtf(fmaxf(v2, 0.0f));
        }

        if (mode == ConvectionMode::NATURELLE || mode == ConvectionMode::COMBINEE) {
            float dT = fmaxf(dT_colonne, 0.0f);
            float v_nat = sqrtf(2.0f * g * fp.beta * dT * params.H_coeur);
            v += v_nat;
        }

        return fmaxf(v, 0.001f);  // minimum 1 mm/s
    }

    // ---- Mise à jour principale ----
    // Appelé chaque fois que grid.cubes est mis à jour (après thermalCompute.applyToGrid)
    // Modèle 1D colonne par colonne, de bas en haut (row décroissant = haut)
    // On suppose row=0 = haut, row=rows-1 = bas (entrée fluide)
    void update(const GridData& grid) {
        if (!initialized) return;

        // Masque des assemblages
        std::vector<int> mask(grid.rows * grid.cols, 0);
        std::vector<float> T_comb(grid.rows * grid.cols, params.T_inlet);
        for (const auto& c : grid.cubes) {
            int idx = c.row * grid.cols + c.col_idx;
            mask[idx]  = 1;
            T_comb[idx] = c.temperature;
        }

        float dz = params.H_coeur / fmaxf(1.0f, (float)grid.rows);  // hauteur d'une cellule

        // Pour chaque colonne (col fixe), le fluide monte de row=(rows-1) vers row=0
        for (int col = 0; col < grid.cols; ++col) {

            float T_f = params.T_inlet;  // température fluide entrée (bas)

            // ΔT colonne pour thermosiphon : estimer depuis T moy de la colonne
            float T_moy_col = 0.0f; int n_col = 0;
            for (int row = 0; row < grid.rows; ++row) {
                int idx = row * grid.cols + col;
                if (mask[idx]) { T_moy_col += T_comb[idx]; ++n_col; }
            }
            float dT_col = (n_col > 0) ? (T_moy_col/n_col - params.T_inlet) : 0.0f;

            // Propriétés au point d'entrée
            FluidProps fp = getProps(params.fluid, params.T_inlet, params.P_bar);
            float v  = calcVitesse(fp, dT_col, params.convMode);
            float mdot = fp.rho * v * params.A_canal;  // kg/s

            // Montée de bas (row=rows-1) vers haut (row=0)
            for (int row = grid.rows - 1; row >= 0; --row) {
                int idx = row * grid.cols + col;

                T_fluid[row][col] = T_f;
                v_fluid[row][col] = v;

                if (!mask[idx]) continue;

                // Flux de chaleur échangé avec le combustible
                // q = h·P_mouillé·dz·(T_comb - T_fluide)
                // h via corrélation Dittus-Boelter : Nu = 0.023·Re^0.8·Pr^0.4
                fp = getProps(params.fluid, T_f, params.P_bar);
                float Re  = fp.rho * v * params.D_h / fmaxf(fp.mu, 1e-10f);
                float Nu  = 0.023f * powf(fmaxf(Re, 100.0f), 0.8f)
                                   * powf(fmaxf(fp.Pr, 0.01f), 0.4f);
                float h   = Nu * fp.k / params.D_h;
                float P_mouille = 4.0f * params.A_canal / params.D_h;  // périmètre mouillé
                float q_echange = h * P_mouille * dz * (T_comb[idx] - T_f);

                // ΔT fluide sur cette cellule
                float dT_f = q_echange / fmaxf(mdot * fp.cp, 1e-3f);
                T_f += dT_f;

                // Mise à jour vitesse avec propriétés locales
                v = calcVitesse(fp, T_f - params.T_inlet, params.convMode);
                mdot = fp.rho * v * params.A_canal;
            }
        }
    }

    // ---- T fluide à une position ----
    float getTfluid(int row, int col) const {
        if (row < 0 || row >= (int)T_fluid.size()) return params.T_inlet;
        if (col < 0 || col >= (int)T_fluid[row].size()) return params.T_inlet;
        return T_fluid[row][col];
    }

    float getVfluid(int row, int col) const {
        if (row < 0 || row >= (int)v_fluid.size()) return 0.0f;
        if (col < 0 || col >= (int)v_fluid[row].size()) return 0.0f;
        return v_fluid[row][col];
    }
};