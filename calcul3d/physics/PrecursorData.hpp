#pragma once
// ============================================================
//  PrecursorData.hpp  —  Données cinétiques neutrons retardés
//
//  6 groupes de précurseurs standard (données IAEA-TECDOC-1234)
//
//  Pour chaque groupe i :
//    β_i   = fraction de neutrons retardés du groupe i
//    λ_i   = constante de décroissance (s⁻¹)
//
//  Équation de précurseur (par cellule) :
//    dCᵢ/dt = (βᵢ/k) · Fsrc - λᵢ · Cᵢ
//
//  Contribution au flux :
//    source_retardée = Σᵢ λᵢ·Cᵢ   (ajoutée au terme de fission)
//
//  β_total = Σ βᵢ ≈ 0.0065 (U-235 thermique)
//           ≈ 0.0021 (Pu-239, RNR)
// ============================================================
#include "../core/GridData.hpp"  // ReactorType

struct PrecursorGroup {
    float beta;    // fraction neutrons retardés (sans unité)
    float lambda;  // constante décroissance (s⁻¹)
};

struct PrecursorData {
    static constexpr int N_GROUPS = 6;

    // ── Données par technologie ──────────────────────────────
    // Source : IAEA-TECDOC-1234, Keepin (1965), Brady & England (1989)

    // U-235 spectre thermique — REP, CANDU, RHT
    static constexpr PrecursorGroup U235_thermal[N_GROUPS] = {
        {0.000215f, 0.0124f},  // groupe 1 : T½ = 55.7 s
        {0.001424f, 0.0305f},  // groupe 2 : T½ = 22.7 s
        {0.001274f, 0.111f },  // groupe 3 : T½ =  6.2 s
        {0.002568f, 0.301f },  // groupe 4 : T½ =  2.3 s
        {0.000748f, 1.14f  },  // groupe 5 : T½ =  0.61 s
        {0.000273f, 3.01f  },  // groupe 6 : T½ =  0.23 s
        // β_total = 0.006502
    };

    // U-238 + Pu-239 spectre rapide — RNR-Na, RNR-Pb
    // β_total ≈ 0.0035 (mix U-238 fissile rapide + Pu-239)
    static constexpr PrecursorGroup U238Pu_fast[N_GROUPS] = {
        {0.000070f, 0.0128f},
        {0.000560f, 0.0318f},
        {0.000520f, 0.119f },
        {0.001060f, 0.321f },
        {0.000360f, 1.21f  },
        {0.000140f, 3.29f  },
        // β_total = 0.002710
    };

    // ── Sélection par type de réacteur ───────────────────────
    static const PrecursorGroup* get(ReactorType rt) {
        switch(rt) {
            case ReactorType::RNR_NA:
            case ReactorType::RNR_PB:
                return U238Pu_fast;
            default:  // REP, CANDU, RHT
                return U235_thermal;
        }
    }

    static float beta_total(ReactorType rt) {
        const auto* g = get(rt);
        float b = 0.0f;
        for (int i=0; i<N_GROUPS; ++i) b += g[i].beta;
        return b;
    }

    // Période de prompt criticité (secondes)
    // Λ = 1 / (v2 * SigA_th)  ≈ 50 µs REP, 1 µs RNR
    static float prompt_lifetime(ReactorType rt) {
        switch(rt) {
            case ReactorType::RNR_NA:
            case ReactorType::RNR_PB: return 0.4e-6f;   // ~0.4 µs
            case ReactorType::CANDU:  return 80.0e-6f;  // ~80 µs
            case ReactorType::RHT:    return 100.0e-6f; // ~100 µs
            default:                  return 50.0e-6f;  // ~50 µs REP
        }
    }
};

// Définitions des constexpr (C++17 inline)
inline constexpr PrecursorGroup PrecursorData::U235_thermal[PrecursorData::N_GROUPS];
inline constexpr PrecursorGroup PrecursorData::U238Pu_fast[PrecursorData::N_GROUPS];