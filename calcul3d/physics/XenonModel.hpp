#pragma once
// ============================================================
//  XenonModel.hpp  v1.0
//
//  Cinétique spatiale Xénon-135 / Iode-135 / Samarium-149
//
//  PHYSIQUE :
//  ─────────
//  L'Iode-135 est produit par fission (via Te-135 ~instable) et
//  décroît en Xé-135. Le Xé-135 est le poison neutronique le plus
//  important en réacteur thermique :
//    σ_a(Xe-135) = 2.65×10⁶ barn  (group thermique)
//
//  Xénon-135 : mécanisme critique
//    - Pic xénon post-arrêt : Xe monte (I décroît → Xe)
//      alors que φ=0 (Xe ne brûle plus)
//    - "Creuse" la réactivité de −15 000 à −30 000 pcm
//    - Oscillations spatiales à pleine puissance (instabilité Xe)
//    - Mécanisme de l'accident de Tchernobyl (ignorance du creux Xe)
//
//  ÉQUATIONS (par cellule) :
//
//    dI/dt = γI·νΣF·φ  −  λI·I
//          = γI·Fsrc   −  λI·I
//
//    dXe/dt = (γXe·νΣF·φ + λI·I)  −  (λXe + σXe·φ₂)·Xe
//           = (γXe·Fsrc + λI·I)    −  (λXe + σXe·φ₂)·Xe
//
//    dSm/dt = γPm·νΣF·φ  −  σSm·φ₂·Sm
//           (Pm-149 → Sm-149, T½(Pm)=53h >> T½(Xe))
//
//  PARAMÈTRES IAEA (données nucléaires Standard) :
//    γI   = 0.0638  (rendement iode, fraction fissions → I-135)
//    γXe  = 0.00228 (rendement xénon direct)
//    λI   = 2.87×10⁻⁵ s⁻¹  (T½ = 6.7h)
//    λXe  = 2.09×10⁻⁵ s⁻¹  (T½ = 9.2h)
//    σXe  = 2.65×10⁶ barns = 2.65×10⁻¹⁸ cm²
//    γPm  = 0.01080 (rendement prométhium)
//    σSm  = 4.1×10⁴ barns = 4.1×10⁻²⁰ cm²
//
//  INTÉGRATION : semi-implicite
//    I^(n+1)  = (I^n  + dt·γI·Fsrc)   / (1 + dt·λI)
//    Xe^(n+1) = (Xe^n + dt·(γXe·Fsrc + λI·I^(n+1)))
//             / (1 + dt·(λXe + σXe·φ₂))
//    Sm^(n+1) = (Sm^n + dt·γPm·Fsrc)
//             / (1 + dt·σSm·φ₂)
//
//  EFFET SUR LES XS :
//    ΔΣA2 += σXe·Xe_density + σSm·Sm_density  (cm⁻¹)
//    Xe_density = Xe × φ_norm / V_cell  (normalisé → cm⁻³ relatif)
//
//  INTERFACE :
//    xeModel.init(total2d)
//    xeModel.step(dt, phi2_flat, nuSF1, nuSF2, phi1, xs)
//    xeModel.applyToXS(xs_SigR2)   → modifie SigR2 de chaque cellule
//    xeModel.xenonPoisoning(i)     → ρ en pcm pour cellule i
//    xeModel.totalPoisoning()      → ρ total en pcm
// ============================================================
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include "../physics/NeutronCrossSection.hpp"
#include <cstdint>


struct XenonModel {

    // ── Données nucléaires IAEA ───────────────────────────────
    static constexpr float GAMMA_I  = 0.0638f;      // rendement I-135
    static constexpr float GAMMA_XE = 0.00228f;     // rendement Xe-135 direct
    static constexpr float LAMBDA_I  = 2.87e-5f;    // s⁻¹  T½=6.7h
    static constexpr float LAMBDA_XE = 2.09e-5f;    // s⁻¹  T½=9.2h
    // σXe en cm² : 2.65×10⁶ barn × 1×10⁻²⁴ cm²/barn = 2.65×10⁻¹⁸ cm²
    static constexpr float SIGMA_XE  = 2.65e-18f;   // cm²
    static constexpr float GAMMA_PM  = 0.01080f;    // rendement Pm-149
    static constexpr float SIGMA_SM  = 4.10e-20f;   // cm²

    // ── État spatial ──────────────────────────────────────────
    std::vector<float> iodine;    // I-135 (unité : cm⁻³, normalisé φ₀)
    std::vector<float> xenon;     // Xe-135
    std::vector<float> samarium;  // Sm-149
    std::vector<float> xe_dSigA2; // ΔΣA₂ induit par Xe+Sm (cm⁻¹)

    int N = 0;
    bool active = false;

    // ── Métriques globales ────────────────────────────────────
    float xe_avg     = 0.0f;  // concentration Xe moyenne (u.a.)
    float reactivity_xe_pcm = 0.0f;  // empoisonnement total estimé (pcm)

    // ── Paramètre de normalisation ────────────────────────────
    // phi_ref = valeur de φ₂ à pleine puissance nominale
    // Permet de travailler en unités relatives
    float phi2_ref = 1.0f;

    // ── Initialisation ────────────────────────────────────────
    void init(int total2d, float phi2_nominal = 1.0f) {
        N = total2d;
        phi2_ref = (phi2_nominal > 1e-20f) ? phi2_nominal : 1.0f;

        iodine.assign(N, 0.0f);
        xenon.assign(N, 0.0f);
        samarium.assign(N, 0.0f);
        xe_dSigA2.assign(N, 0.0f);

        active = true;
        std::cout << "[XenonModel] Init : " << N
                  << " cellules, phi2_ref=" << phi2_ref << "\n";
    }

    // ── Equilibrium initial (à pleine puissance) ─────────────
    //  dI/dt=0 → I_eq = γI·Fsrc/λI
    //  dXe/dt=0 → Xe_eq = (γXe+γI)·Fsrc / (λXe + σXe·φ₂)
    //  dSm/dt=0 → Sm_eq = γPm·Fsrc / (σSm·φ₂)
    void setEquilibrium(const std::vector<float>& Fsrc,
                        const std::vector<float>& phi2)
    {
        for (int i = 0; i < N; ++i) {
            float F = Fsrc[i];
            float p2 = phi2[i];
            float phi2n = p2 / phi2_ref;  // normalisé

            iodine[i]   = (LAMBDA_I > 0)
                        ? GAMMA_I * F / LAMBDA_I : 0.0f;

            float denom = LAMBDA_XE + SIGMA_XE * phi2n;
            xenon[i]    = (denom > 1e-30f)
                        ? (GAMMA_XE + GAMMA_I) * F / denom : 0.0f;

            float denom_sm = SIGMA_SM * fmaxf(phi2n, 1e-10f);
            samarium[i] = (denom_sm > 1e-30f)
                        ? GAMMA_PM * F / denom_sm : 0.0f;
        }
        _recomputeDeltaSigA2(phi2);
        _updateMetrics();
        std::cout << "[XenonModel] Equilibrium : Xe_avg=" << xe_avg
                  << "  ρ_Xe=" << reactivity_xe_pcm << " pcm\n";
    }

    // ── Pas de temps ─────────────────────────────────────────
    //  Fsrc[i] = νΣF1·φ1 + νΣF2·φ2  (source fission locale)
    //  phi2[i] = flux thermique local (unité cohérente avec phi2_ref)
    void step(float dt,
              const std::vector<float>& Fsrc,
              const std::vector<float>& phi2)
    {
        if (!active || N == 0) return;

        for (int i = 0; i < N; ++i) {
            float F   = Fsrc[i];
            float p2n = phi2[i] / phi2_ref;  // normalisé

            // ── Iode : semi-implicite ─────────────────────────
            float I_new = (iodine[i] + dt * GAMMA_I * F)
                        / (1.0f + dt * LAMBDA_I);
            I_new = fmaxf(0.0f, I_new);

            // ── Xénon : semi-implicite ────────────────────────
            float Xe_denom = 1.0f + dt * (LAMBDA_XE + SIGMA_XE * p2n);
            float Xe_new   = (xenon[i]
                              + dt * (GAMMA_XE * F + LAMBDA_I * I_new))
                           / Xe_denom;
            Xe_new = fmaxf(0.0f, Xe_new);

            // ── Samarium : semi-implicite ─────────────────────
            float Sm_denom = 1.0f + dt * SIGMA_SM * p2n;
            float Sm_new   = (samarium[i] + dt * GAMMA_PM * F)
                           / Sm_denom;
            Sm_new = fmaxf(0.0f, Sm_new);

            iodine[i]   = I_new;
            xenon[i]    = Xe_new;
            samarium[i] = Sm_new;
        }

        _recomputeDeltaSigA2(phi2);
        _updateMetrics();
    }

    // ── Application aux XS (modifie SigR2) ──────────────────
    //  ΔΣA2 = σXe · [Xe] + σSm · [Sm]
    //  On l'ajoute à SigR2 (retrait g2) dans NeutronCompute
    //
    //  Appeler après step(), avant rebuildXS() ou en surcharge de SigR2
    void applyToSigR2(std::vector<float>& xs_SigR2) const {
        if ((int)xs_SigR2.size() < N) return;
        for (int i = 0; i < N; ++i)
            xs_SigR2[i] += xe_dSigA2[i];
    }

    // ── Empoisonnement local ─────────────────────────────────
    //  Estimation pcm via formule de Nordheim-Fuchs simplifiée :
    //  Δρ ≈ −ΔΣA2 / (νΣF2 - ΣR2)  × 1e5 (en pcm)
    //  → utiliser comme indicateur qualitatif uniquement
    float xenonPoisoning_ua(int i) const {
        if (i < 0 || i >= N) return 0.0f;
        return xe_dSigA2[i];
    }

    float totalPoisoning_pcm() const { return reactivity_xe_pcm; }

    // ── Reset (post-arrêt, t=0) ──────────────────────────────
    void reset() {
        std::fill(iodine.begin(),   iodine.end(),   0.0f);
        std::fill(xenon.begin(),    xenon.end(),    0.0f);
        std::fill(samarium.begin(), samarium.end(), 0.0f);
        std::fill(xe_dSigA2.begin(),xe_dSigA2.end(),0.0f);
        xe_avg = 0.0f;
        reactivity_xe_pcm = 0.0f;
    }

    // ── Reset partiel : garder les concentrations existantes ─
    //  (pour continuer depuis un état Xe≠0 après restart)
    void keepState() { /* rien — l'état est préservé */ }

private:
    void _recomputeDeltaSigA2(const std::vector<float>& phi2) {
        for (int i = 0; i < N; ++i) {
            float p2n = phi2[i] / phi2_ref;
            // ΔΣA2 en cm⁻¹
            // [Xe] et [Sm] en unités relatives → × φ₂_ref pour cm⁻³ fictif
            // L'important est la cohérence avec les XS de NeutronCrossSection
            // qui sont en cm⁻¹. On calibre :
            //   σXe × N_atoms = ΔΣA2  →  N ≈ 1 (normalisé)
            //   A pleine puissance, Δρ_Xe ≈ −2000 pcm pour REP
            xe_dSigA2[i] = SIGMA_XE * xenon[i]
                         + SIGMA_SM * samarium[i];
            // Clampage physique : max Δρ ≈ −30 000 pcm → ΔΣA2 ≈ 0.3 cm⁻¹
            xe_dSigA2[i] = fminf(xe_dSigA2[i], 0.30f);
        }
    }

    void _updateMetrics() {
        float sum = 0.0f;
        float dSigSum = 0.0f;
        int cnt = 0;
        for (int i = 0; i < N; ++i) {
            sum     += xenon[i];
            dSigSum += xe_dSigA2[i];
            cnt++;
        }
        xe_avg = (cnt > 0) ? sum / cnt : 0.0f;

        // Estimation empoisonnement global :
        // Δρ ≈ −<ΔΣA2> / <ΣR2_fuel>  avec ΣR2_fuel ≈ 0.2 cm⁻¹ (REP)
        // × 1e5 pour pcm
        float SigR2_ref = 0.20f;
        float dSig_avg  = (cnt > 0) ? dSigSum / cnt : 0.0f;
        reactivity_xe_pcm = -(dSig_avg / SigR2_ref) * 1e5f;
    }
};