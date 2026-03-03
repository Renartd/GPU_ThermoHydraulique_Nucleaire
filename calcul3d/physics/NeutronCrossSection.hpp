#pragma once
// ============================================================
//  NeutronCrossSection.hpp
//  Sections efficaces neutroniques paramétriques 2 groupes
//
//  Groupe 1 : neutrons rapides  (E > 0.625 eV)
//  Groupe 2 : neutrons thermiques (E < 0.625 eV)
//
//  Parametres :
//    epsilon     = N_U235/(N_U235+N_U238)  [0..1]
//    T_fuel      = temperature combustible (deg C) -> Doppler
//    rho_mod_rel = densite relative moderateur (1.0 = nominal)
//    ReactorType = REP / CANDU / RNR_NA / RNR_PB / RHT
//    ZoneType    = FUEL / MODERATOR / REFLECTOR / CONTROL_ROD / COOLANT / VOID
// ============================================================
#include <cmath>
#include <string>

enum class ReactorType { REP, CANDU, RNR_NA, RNR_PB, RHT };
enum class ZoneType    { FUEL, MODERATOR, REFLECTOR, CONTROL_ROD, COOLANT, VOID };

// Sections efficaces 2 groupes pour une zone
struct XS2G {
    float D[2]      = {1.5f,   0.4f  };  // cm   - coeff diffusion
    float SigR[2]   = {0.02f,  0.10f };  // cm-1 - section retrait
    float SigS12    = 0.018f;             // cm-1 - ralentissement g1->g2
    float nuSigF[2] = {0.0f,   0.0f  };  // cm-1 - production fission
    float chi[2]    = {1.0f,   0.0f  };  // spectre fission (g1=rapide)
    float SigA[2]   = {0.005f, 0.08f };  // cm-1 - absorption pure
};

struct NeutronCrossSection {

    // ----------------------------------------------------------------
    //  Combustible UO2 / MOX
    // ----------------------------------------------------------------
    static XS2G fuel(ReactorType rt, float epsilon, float T_fuel,
                     float rho_mod_rel = 1.0f)
    {
        XS2G xs;

        // Effet Doppler : sigma_a(U238) proportionnel a 1/sqrt(T)
        // Reference T=300 C -> sqrt(573.15 K)
        float sqrtT   = sqrtf(T_fuel + 273.15f);
        float sqrtRef = sqrtf(573.15f);
        float dT      = sqrtT - sqrtRef;
        // Plus il y a d'U238 (1-epsilon), plus l'effet Doppler est fort
        float dopp = 1.0f + 3.5e-3f * dT * (1.0f - epsilon);
        dopp = fmaxf(0.30f, dopp);

        switch (rt) {

        // ---- REP : enrichissement nominal 3.5%, moderateur H2O ----
        case ReactorType::REP: {
            float e = epsilon / 0.035f;
            xs.D[0]      = 1.35f;
            xs.D[1]      = 0.38f * (1.0f + 0.15f * (1.0f - rho_mod_rel));
            xs.SigR[0]   = 0.025f * dopp;
            xs.SigR[1]   = 0.120f * dopp * rho_mod_rel;
            xs.SigS12    = 0.020f * rho_mod_rel;
            xs.nuSigF[0] = 0.006f * e;
            xs.nuSigF[1] = 0.095f * e;
            xs.chi[0]    = 0.98f;
            xs.chi[1]    = 0.02f;
            xs.SigA[0]   = 0.008f;
            xs.SigA[1]   = 0.075f * dopp;
            break;
        }

        // ---- CANDU : enrichissement naturel ~0.72%, moderateur D2O ----
        case ReactorType::CANDU: {
            float e = epsilon / 0.007f;
            xs.D[0]      = 1.50f;
            xs.D[1]      = 0.95f * (1.0f + 0.12f * (1.0f - rho_mod_rel));
            xs.SigR[0]   = 0.018f * dopp;
            xs.SigR[1]   = 0.045f * dopp;
            xs.SigS12    = 0.015f * rho_mod_rel;
            xs.nuSigF[0] = 0.004f * e;
            xs.nuSigF[1] = 0.085f * e;
            xs.chi[0]    = 0.98f;
            xs.chi[1]    = 0.02f;
            xs.SigA[0]   = 0.004f;
            xs.SigA[1]   = 0.030f * dopp;
            break;
        }

        // ---- RNR-Na : enrichissement 15%, spectre rapide, pas de moderateur ----
        case ReactorType::RNR_NA: {
            float e = epsilon / 0.15f;
            xs.D[0]      = 1.60f;
            xs.D[1]      = 1.40f;
            xs.SigR[0]   = 0.030f * dopp;
            xs.SigR[1]   = 0.040f * dopp;
            xs.SigS12    = 0.003f;           // tres peu de ralentissement
            xs.nuSigF[0] = 0.045f * e;       // fissions surtout rapides
            xs.nuSigF[1] = 0.020f * e;
            xs.chi[0]    = 0.99f;
            xs.chi[1]    = 0.01f;
            xs.SigA[0]   = 0.012f;
            xs.SigA[1]   = 0.025f * dopp;
            break;
        }

        // ---- RNR-Pb/Bi : enrichissement 15%, grand D (diffusion Pb) ----
        case ReactorType::RNR_PB: {
            float e = epsilon / 0.15f;
            xs.D[0]      = 2.40f;
            xs.D[1]      = 2.00f;
            xs.SigR[0]   = 0.025f * dopp;
            xs.SigR[1]   = 0.032f * dopp;
            xs.SigS12    = 0.002f;
            xs.nuSigF[0] = 0.040f * e;
            xs.nuSigF[1] = 0.015f * e;
            xs.chi[0]    = 0.99f;
            xs.chi[1]    = 0.01f;
            xs.SigA[0]   = 0.008f;
            xs.SigA[1]   = 0.020f * dopp;
            break;
        }

        // ---- RHT : enrichissement 9%, moderateur graphite, caloporteur He ----
        case ReactorType::RHT: {
            float e = epsilon / 0.09f;
            xs.D[0]      = 2.00f;
            xs.D[1]      = 0.80f;
            xs.SigR[0]   = 0.020f * dopp;
            xs.SigR[1]   = 0.080f * dopp;
            xs.SigS12    = 0.012f * rho_mod_rel;
            xs.nuSigF[0] = 0.008f * e;
            xs.nuSigF[1] = 0.090f * e;
            xs.chi[0]    = 0.98f;
            xs.chi[1]    = 0.02f;
            xs.SigA[0]   = 0.006f;
            xs.SigA[1]   = 0.060f * dopp;
            break;
        }
        }
        return xs;
    }

    // ----------------------------------------------------------------
    //  Moderateur
    //  rho_rel : densite relative (varie avec T, vide caloporteur)
    // ----------------------------------------------------------------
    static XS2G moderator(ReactorType rt, float rho_rel = 1.0f)
    {
        XS2G xs;
        xs.nuSigF[0] = xs.nuSigF[1] = 0.0f;
        xs.chi[0]    = xs.chi[1]    = 0.0f;
        float r = fmaxf(rho_rel, 0.01f);

        switch (rt) {
        // H2O : fort Sigma_s, Sigma_a modere
        case ReactorType::REP:
            xs.D[0]    = 1.20f / r;   xs.D[1]    = 0.18f / r;
            xs.SigR[0] = 0.040f * r;  xs.SigR[1] = 0.022f * r;
            xs.SigS12  = 0.025f * r;
            xs.SigA[0] = 0.001f;      xs.SigA[1] = 0.018f * r;
            break;
        // D2O : Sigma_a tres faible (~100x moins que H2O)
        case ReactorType::CANDU:
            xs.D[0]    = 1.40f / r;   xs.D[1]    = 0.90f / r;
            xs.SigR[0] = 0.015f * r;  xs.SigR[1] = 0.0003f * r;
            xs.SigS12  = 0.015f * r;
            xs.SigA[0] = 0.0005f;     xs.SigA[1] = 0.00015f * r;
            break;
        // Sodium : faible moderation, Sigma_a petit
        case ReactorType::RNR_NA:
            xs.D[0]    = 1.20f / r;   xs.D[1]    = 1.10f / r;
            xs.SigR[0] = 0.008f * r;  xs.SigR[1] = 0.008f * r;
            xs.SigS12  = 0.001f * r;
            xs.SigA[0] = 0.003f;      xs.SigA[1] = 0.003f;
            break;
        // Plomb-Bismuth : grand D, Sigma_a faible
        case ReactorType::RNR_PB:
            xs.D[0]    = 2.00f;        xs.D[1]    = 1.80f;
            xs.SigR[0] = 0.006f * r;   xs.SigR[1] = 0.004f * r;
            xs.SigS12  = 0.001f * r;
            xs.SigA[0] = 0.001f;       xs.SigA[1] = 0.001f;
            break;
        // Graphite : Sigma_a tres faible, bon ralentissement (mais lent)
        case ReactorType::RHT:
            xs.D[0]    = 2.50f;        xs.D[1]    = 0.90f;
            xs.SigR[0] = 0.003f;       xs.SigR[1] = 0.0004f;
            xs.SigS12  = 0.004f;
            xs.SigA[0] = 0.0003f;      xs.SigA[1] = 0.0003f;
            break;
        }
        return xs;
    }

    // ----------------------------------------------------------------
    //  Reflecteur
    // ----------------------------------------------------------------
    static XS2G reflector(ReactorType rt)
    {
        XS2G xs;
        xs.nuSigF[0] = xs.nuSigF[1] = 0.0f;
        xs.chi[0]    = xs.chi[1]    = 0.0f;

        switch (rt) {
        // REP : acier inoxydable 316L + eau
        case ReactorType::REP:
            xs.D[0]    = 0.80f;  xs.D[1]    = 0.25f;
            xs.SigR[0] = 0.08f;  xs.SigR[1] = 0.15f;
            xs.SigS12  = 0.006f;
            xs.SigA[0] = 0.020f; xs.SigA[1] = 0.08f;
            break;
        // CANDU : reflecteur D2O (meme que moderateur)
        case ReactorType::CANDU:
            return moderator(ReactorType::CANDU, 1.0f);
        // RNR-Na et RNR-Pb : acier ou meme caloporteur
        case ReactorType::RNR_NA:
        case ReactorType::RNR_PB:
            xs.D[0]    = 0.70f;  xs.D[1]    = 0.60f;
            xs.SigR[0] = 0.10f;  xs.SigR[1] = 0.08f;
            xs.SigS12  = 0.002f;
            xs.SigA[0] = 0.025f; xs.SigA[1] = 0.020f;
            break;
        // RHT : reflecteur graphite (meme que moderateur)
        case ReactorType::RHT:
            return moderator(ReactorType::RHT, 1.0f);
        }
        return xs;
    }

    // ----------------------------------------------------------------
    //  Barre de controle B4C
    //  insertFraction : 0=sortie totale, 1=inseree totalement
    // ----------------------------------------------------------------
    static XS2G controlRod(float insertFraction = 1.0f)
    {
        XS2G xs;
        xs.nuSigF[0] = xs.nuSigF[1] = 0.0f;
        xs.chi[0]    = xs.chi[1]    = 0.0f;
        float f = fmaxf(0.0f, fminf(1.0f, insertFraction));
        // B4C : sigma_a_thermique ~ 760 barns pour B-10
        xs.D[0]    = 0.50f;
        xs.D[1]    = 0.15f;
        xs.SigR[0] = 0.10f + 0.80f * f;
        xs.SigR[1] = 0.20f + 3.00f * f;  // forte absorption thermique
        xs.SigS12  = 0.005f;
        xs.SigA[0] = 0.08f + 0.70f * f;
        xs.SigA[1] = 0.18f + 2.80f * f;
        return xs;
    }

    // ----------------------------------------------------------------
    //  Utilitaires
    // ----------------------------------------------------------------
    static float nominalEnrichment(ReactorType rt)
    {
        switch (rt) {
        case ReactorType::REP:    return 0.035f;
        case ReactorType::CANDU:  return 0.007f;
        case ReactorType::RNR_NA: return 0.150f;
        case ReactorType::RNR_PB: return 0.150f;
        case ReactorType::RHT:    return 0.090f;
        }
        return 0.035f;
    }

    static const char* reactorName(ReactorType rt)
    {
        switch (rt) {
        case ReactorType::REP:    return "REP (H2O)";
        case ReactorType::CANDU:  return "CANDU (D2O)";
        case ReactorType::RNR_NA: return "RNR-Na";
        case ReactorType::RNR_PB: return "RNR-Pb/Bi";
        case ReactorType::RHT:    return "RHT (He+Graphite)";
        }
        return "?";
    }
};