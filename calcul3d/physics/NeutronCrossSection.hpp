#pragma once
// ============================================================
//  NeutronCrossSection.hpp  v2.0
//
//  NOUVEAUTÉS v2 :
//    1. fuelFull() : XS multi-paramétriques REP
//         f(T_fuel, T_mod, rho_mod, C_bore_ppm)
//    2. Coefficient vide sodium paramétré (RNR-Na)
//    3. feedbacks() : retourne les coefficients de température
//    4. Compatibilité totale v1 : fuel() délègue vers fuelFull()
// ============================================================
#include <cmath>
#include <string>
#include <algorithm>

enum class ReactorType { REP, CANDU, RNR_NA, RNR_PB, RHT };
enum class ZoneType    { FUEL, MODERATOR, REFLECTOR, CONTROL_ROD, COOLANT, VOID };

struct XS2G {
    float D[2]      = {1.5f,   0.4f  };
    float SigR[2]   = {0.02f,  0.10f };
    float SigS12    = 0.018f;
    float nuSigF[2] = {0.0f,   0.0f  };
    float chi[2]    = {1.0f,   0.0f  };
    float SigA[2]   = {0.005f, 0.08f };
};

struct NeutronCrossSection {

    // ============================================================
    //  COMBUSTIBLE v2 — multi-paramétrique
    // ============================================================
    static XS2G fuelFull(ReactorType rt, float epsilon,
                         float T_fuel,
                         float rho_mod_rel = 1.0f,
                         float T_mod       = 290.0f,
                         float C_bore_ppm  = 0.0f)
    {
        float sqrtT    = sqrtf(T_fuel  + 273.15f);
        float sqrtTref = sqrtf(573.15f);
        float doppler  = 1.0f + 3.5e-3f * (sqrtT - sqrtTref)
                              * (1.0f - epsilon);
        doppler = std::max(0.30f, doppler);

        XS2G xs;

        switch (rt) {

        case ReactorType::REP: {
            float e   = epsilon / 0.035f;
            float rho = std::max(0.01f, rho_mod_rel);

            xs.D[0]      = 1.35f;
            xs.D[1]      = 0.38f * (1.0f + 0.15f*(1.0f - rho));
            xs.SigR[0]   = 0.033f * doppler;
            xs.SigR[1]   = 0.200f * doppler * rho;
            xs.SigS12    = 0.0175f * rho;
            xs.nuSigF[0] = 0.0082f * e;
            xs.nuSigF[1] = 0.3230f * e;
            xs.chi[0]    = 0.98f; xs.chi[1] = 0.02f;
            xs.SigA[0]   = 0.010f * doppler;
            xs.SigA[1]   = 0.160f * doppler * rho;

            // Correction T_moderateur (alphaM ≈ -30 pcm/°C)
            float dTm = T_mod - 290.0f;
            xs.SigR[1] *= (1.0f + 3.0e-4f * dTm);
            xs.SigS12  *= (1.0f - 2.0e-4f * dTm);
            xs.D[1]    *= (1.0f - 1.5e-4f * dTm);

            // Correction bore (alphaBore ≈ -10 pcm/ppm)
            if (C_bore_ppm > 0.0f) {
                xs.SigR[1] += 2.0e-5f * C_bore_ppm;
                xs.SigA[1] += 1.8e-5f * C_bore_ppm;
            }
            break;
        }

        case ReactorType::CANDU: {
            float e = epsilon / 0.007f;
            xs.D[0]      = 1.50f;
            xs.D[1]      = 0.95f * (1.0f + 0.12f*(1.0f - rho_mod_rel));
            xs.SigR[0]   = 0.018f * doppler;
            xs.SigR[1]   = 0.045f * doppler;
            xs.SigS12    = 0.015f * rho_mod_rel;
            xs.nuSigF[0] = 0.0030f * e;
            xs.nuSigF[1] = 0.1978f * e;
            xs.chi[0]    = 0.98f; xs.chi[1] = 0.02f;
            xs.SigA[0]   = 0.004f;
            xs.SigA[1]   = 0.030f * doppler;
            break;
        }

        case ReactorType::RNR_NA: {
            float e   = epsilon / 0.15f;
            float rho = std::max(0.01f, rho_mod_rel);
            // CVS paramétré : vide Na → D augmente (fuite), SigR diminue (spec)
            xs.D[0]      = 1.60f * (1.0f + 0.10f*(1.0f - rho));
            xs.D[1]      = 1.40f * (1.0f + 0.08f*(1.0f - rho));
            xs.SigR[0]   = 0.0312f * doppler * rho;
            xs.SigR[1]   = 0.040f  * doppler * rho;
            xs.SigS12    = 0.003f  * rho;
            xs.nuSigF[0] = 0.0340f * e;
            xs.nuSigF[1] = 0.0050f * e;
            xs.chi[0]    = 0.99f; xs.chi[1] = 0.01f;
            xs.SigA[0]   = 0.012f;
            xs.SigA[1]   = 0.025f * doppler;
            break;
        }

        case ReactorType::RNR_PB: {
            float e = epsilon / 0.15f;
            xs.D[0]      = 2.40f; xs.D[1]      = 2.00f;
            xs.SigR[0]   = 0.0275f * doppler;
            xs.SigR[1]   = 0.032f  * doppler;
            xs.SigS12    = 0.002f;
            xs.nuSigF[0] = 0.0300f * e;
            xs.nuSigF[1] = 0.0140f * e;
            xs.chi[0]    = 0.99f; xs.chi[1] = 0.01f;
            xs.SigA[0]   = 0.008f;
            xs.SigA[1]   = 0.020f * doppler;
            break;
        }

        case ReactorType::RHT: {
            float e = epsilon / 0.09f;
            xs.D[0]      = 2.00f; xs.D[1]      = 0.80f;
            xs.SigR[0]   = 0.020f * doppler;
            xs.SigR[1]   = 0.080f * doppler;
            xs.SigS12    = 0.012f * rho_mod_rel;
            xs.nuSigF[0] = 0.0050f * e;
            xs.nuSigF[1] = 0.1064f * e;
            xs.chi[0]    = 0.98f; xs.chi[1] = 0.02f;
            xs.SigA[0]   = 0.006f;
            xs.SigA[1]   = 0.060f * doppler;
            break;
        }
        }
        return xs;
    }

    // v1 compat
    static XS2G fuel(ReactorType rt, float epsilon, float T_fuel,
                     float rho_mod_rel = 1.0f)
    { return fuelFull(rt, epsilon, T_fuel, rho_mod_rel, 290.0f, 0.0f); }

    // ============================================================
    //  MODÉRATEUR
    // ============================================================
    static XS2G moderator(ReactorType rt, float rho_rel = 1.0f)
    {
        XS2G xs;
        xs.nuSigF[0] = xs.nuSigF[1] = 0.0f;
        xs.chi[0]    = xs.chi[1]    = 0.0f;
        float r = std::max(rho_rel, 0.01f);
        switch (rt) {
        case ReactorType::REP:
            xs.D[0]=1.20f/r;   xs.D[1]=0.18f/r;
            xs.SigR[0]=0.040f*r; xs.SigR[1]=0.022f*r;
            xs.SigS12=0.025f*r;
            xs.SigA[0]=0.001f; xs.SigA[1]=0.018f*r;
            break;
        case ReactorType::CANDU:
            xs.D[0]=1.40f/r;   xs.D[1]=0.90f/r;
            xs.SigR[0]=0.015f*r; xs.SigR[1]=0.0003f*r;
            xs.SigS12=0.015f*r;
            xs.SigA[0]=0.0005f; xs.SigA[1]=0.00015f*r;
            break;
        case ReactorType::RNR_NA:
            xs.D[0]=1.20f/r;   xs.D[1]=1.10f/r;
            xs.SigR[0]=0.008f*r; xs.SigR[1]=0.008f*r;
            xs.SigS12=0.001f*r;
            xs.SigA[0]=0.003f; xs.SigA[1]=0.003f;
            break;
        case ReactorType::RNR_PB:
            xs.D[0]=2.00f; xs.D[1]=1.80f;
            xs.SigR[0]=0.006f*r; xs.SigR[1]=0.004f*r;
            xs.SigS12=0.001f*r;
            xs.SigA[0]=0.001f; xs.SigA[1]=0.001f;
            break;
        case ReactorType::RHT:
            xs.D[0]=2.50f; xs.D[1]=0.90f;
            xs.SigR[0]=0.003f; xs.SigR[1]=0.0004f;
            xs.SigS12=0.004f;
            xs.SigA[0]=0.0003f; xs.SigA[1]=0.0003f;
            break;
        }
        return xs;
    }

    // ============================================================
    //  RÉFLECTEUR
    // ============================================================
    static XS2G reflector(ReactorType rt)
    {
        XS2G xs;
        xs.nuSigF[0]=xs.nuSigF[1]=0.0f;
        xs.chi[0]=xs.chi[1]=0.0f;
        switch (rt) {
        case ReactorType::REP:
            xs.D[0]=1.30f; xs.D[1]=0.25f;
            xs.SigR[0]=0.025f; xs.SigR[1]=0.020f;
            xs.SigS12=0.020f;
            xs.SigA[0]=0.002f; xs.SigA[1]=0.015f;
            break;
        case ReactorType::CANDU:
            xs.D[0]=1.40f; xs.D[1]=0.90f;
            xs.SigR[0]=0.012f; xs.SigR[1]=0.001f;
            xs.SigS12=0.012f;
            xs.SigA[0]=0.001f; xs.SigA[1]=0.001f;
            break;
        case ReactorType::RNR_NA: case ReactorType::RNR_PB:
            xs.D[0]=2.00f; xs.D[1]=1.80f;
            xs.SigR[0]=0.005f; xs.SigR[1]=0.004f;
            xs.SigS12=0.001f;
            xs.SigA[0]=0.002f; xs.SigA[1]=0.002f;
            break;
        case ReactorType::RHT:
            xs.D[0]=2.80f; xs.D[1]=1.20f;
            xs.SigR[0]=0.003f; xs.SigR[1]=0.001f;
            xs.SigS12=0.005f;
            xs.SigA[0]=0.001f; xs.SigA[1]=0.001f;
            break;
        }
        return xs;
    }

    // ============================================================
    //  BARRE DE CONTRÔLE (B4C)
    // ============================================================
    static XS2G controlRod(float insertFraction = 1.0f)
    {
        XS2G xs;
        xs.nuSigF[0]=xs.nuSigF[1]=0.0f;
        xs.chi[0]=xs.chi[1]=0.0f;
        float f = std::max(0.0f, std::min(1.0f, insertFraction));
        xs.D[0]=0.50f; xs.D[1]=0.15f;
        xs.SigR[0]=0.10f + 0.80f*f;
        xs.SigR[1]=0.20f + 3.00f*f;
        xs.SigS12=0.005f;
        xs.SigA[0]=0.08f + 0.70f*f;
        xs.SigA[1]=0.18f + 2.80f*f;
        return xs;
    }

    // ============================================================
    //  UTILITAIRES
    // ============================================================
    static float nominalEnrichment(ReactorType rt) {
        switch (rt) {
        case ReactorType::REP:    return 0.035f;
        case ReactorType::CANDU:  return 0.007f;
        case ReactorType::RNR_NA: return 0.150f;
        case ReactorType::RNR_PB: return 0.150f;
        case ReactorType::RHT:    return 0.090f;
        }
        return 0.035f;
    }

    struct FeedbackCoeffs {
        float alpha_doppler;  // pcm/°C
        float alpha_mod;      // pcm/°C
        float alpha_void;     // pcm/% vide
        float alpha_bore;     // pcm/ppm
    };
    static FeedbackCoeffs feedbacks(ReactorType rt) {
        switch (rt) {
        case ReactorType::REP:    return {-3.5f, -30.0f, -50.0f, -10.0f};
        case ReactorType::CANDU:  return {-2.0f,  -5.0f, +15.0f,   0.0f};
        case ReactorType::RNR_NA: return {-1.5f,   0.0f,  -3.0f,   0.0f};
        case ReactorType::RNR_PB: return {-1.0f,   0.0f,  -2.0f,   0.0f};
        case ReactorType::RHT:    return {-4.0f, -12.0f,   0.0f,   0.0f};
        }
        return {-3.5f, -30.0f, -50.0f, -10.0f};
    }

    static std::string name(ReactorType rt) {
        switch (rt) {
        case ReactorType::REP:    return "REP (PWR)";
        case ReactorType::CANDU:  return "CANDU";
        case ReactorType::RNR_NA: return "RNR-Na (SFR)";
        case ReactorType::RNR_PB: return "RNR-Pb (LFR)";
        case ReactorType::RHT:    return "RHT (HTGR)";
        }
        return "Inconnu";
    }
};