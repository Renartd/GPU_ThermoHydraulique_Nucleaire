#pragma once
// ============================================================
//  VulkanCompute — Calcul GPU via Vulkan Compute Shaders
//
//  Prévu :
//    - Initialisation contexte Vulkan (vkbootstrap)
//    - Chargement shaders SPIR-V
//    - Diffusion thermique 2D sur GPU
//    - Résultat → GridData::temperature
//
//  Dépendances futures :
//    sudo apt install libvulkan-dev
//    pip install vkbootstrap  (ou cloner depuis github)
//    sudo apt install glslc   (compilateur GLSL → SPIR-V)
// ============================================================

class VulkanCompute {
public:
    // TODO
    static bool init()    { return false; }
    static void compute() {}
    static void cleanup() {}
};
