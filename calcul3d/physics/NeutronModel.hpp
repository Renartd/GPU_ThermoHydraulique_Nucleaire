#pragma once
// ============================================================
//  NeutronModel — Modèle neutronique (à implémenter)
//
//  Prévu :
//    - Calcul du flux neutronique par assemblage
//    - Couplage avec ThermalModel (rétroaction Doppler)
//    - Interface avec OpenMC (futur)
// ============================================================
#include "../core/GridData.hpp"

class NeutronModel {
public:
    // TODO : calcul du flux neutronique
    static void calculerFlux(GridData& /*grid*/) {
        // Placeholder — sera implémenté avec Vulkan Compute
    }
};
