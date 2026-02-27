// ============================================================
//  main.cpp — Visualiseur Assemblages Nucléaires
//  Rendu : raylib (OpenGL)
//  Ombres : Vulkan RT (ray query) — touche V
//  Dimensions : panneau cliquable — touche P
// ============================================================

#include <raylib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

#include "core/ReactorParams.hpp"
#include "core/GridData.hpp"
#include "core/AssemblageLoader.hpp"
#include "render/Camera.hpp"
#include "render/ColorMap.hpp"
#include "render/CubeRenderer.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/HUD.hpp"
#include "render/DimsPanel.hpp"
#include "physics/ThermalModel.hpp"
#include "compute/VulkanContext.hpp"
#include "compute/ShadowCompute.hpp"

// ============================================================
//  Overlay RT
// ============================================================
inline void drawRTOverlay(int sw, int sh,
                           bool rtAvailable, bool rtEnabled,
                           const LightParams& lp, float lightAngle) {
    int x = 10, y = 170;
    DrawRectangle(x, y, 230, 90, {0,0,0,170});
    DrawRectangleLines(x, y, 230, 90, {80,80,80,255});
    DrawText("RAY TRACING (Vulkan)", x+8, y+8, 13, LIGHTGRAY);
    if (!rtAvailable) {
        DrawText("Non disponible", x+8, y+28, 12, {180,80,80,255});
        return;
    }
    Color stateCol = rtEnabled ? Color{100,255,100,255} : Color{180,80,80,255};
    DrawText(rtEnabled ? "ON  [V]" : "OFF [V]", x+8, y+28, 12, stateCol);
    char buf[64];
    snprintf(buf, sizeof(buf), "Samples : %d  [N/M]", lp.numSamples);
    DrawText(buf, x+8, y+46, 12, LIGHTGRAY);
    snprintf(buf, sizeof(buf), "Lumiere : %.0f deg  [J/K]", lightAngle);
    DrawText(buf, x+8, y+64, 12, {255,220,100,255});
}

// ============================================================
//  MAIN
// ============================================================
int main() {

    const std::string gridPath = "data/Assemblage.txt";
    const std::string csvPath  = "data/temperatures.csv";

    { std::ifstream t(gridPath); if (!t.is_open()) {
        std::cerr << "Fichier introuvable : " << gridPath << "\n"; return 1; } }

    // --- Paramètres réacteur ---
    ReactorParams params = ReactorParams::lireDepuisFichier(gridPath);
    params.saisirConsole();
    std::cout << "Sauvegarder ? (o/n) : ";
    std::string rep; std::getline(std::cin, rep);
    if (rep == "o" || rep == "O") params.sauvegarder(gridPath);

    // --- Chargement grille ---
    auto raw = AssemblageLoader::load(gridPath);
    GridData grid;
    grid.rows = (int)raw.size();
    grid.cols = grid.rows > 0 ? (int)raw[0].size() : 0;

    for (int r = 0; r < grid.rows; ++r)
        for (int c = 0; c < (int)raw[r].size(); ++c) {
            if (!raw[r][c].isAssembly) continue;  // ← ignorer les vides '-'
            Cube cube;
            cube.sym         = raw[r][c].symbol;
            cube.col         = symbolColor(cube.sym); // ← couleur par symbole
            cube.temperature = params.tempEntree;
            cube.flux        = 0.0f;
            cube.row         = r;
            cube.col_idx     = c;
            cube.pos         = {0,0,0}; // sera calculé par rebuildPositions
            grid.cubes.push_back(cube);
        }

    // Calcule les positions initiales selon les dims par défaut
    grid.rebuildPositions();
    std::cout << "[Main] " << grid.cubes.size() << " assemblages\n";

    // --- Thermique ---
    if (!ThermalModel::chargerCSV(csvPath, grid)) {
        ThermalModel::simulerGaussien(grid, params);
        ThermalModel::genererCSVExemple(csvPath, grid, params);
    }

    // --- Vulkan RT ---
    VulkanContext vkCtx;
    ShadowCompute shadowCompute;
    LightParams   lightParams;
    bool rtAvailable = false;

    if (vkCtx.init() && shadowCompute.init(vkCtx, grid)) {
        rtAvailable = true;
        shadowCompute.compute(lightParams);
        std::cout << "[Main] Vulkan RT OK\n";
    } else {
        std::cout << "[Main] RT non dispo\n";
    }
    bool rtEnabled = rtAvailable;

    // --- Raylib ---
    const int SW = 1200, SH = 800;
    InitWindow(SW, SH, "Visualiseur Assemblages Nucleaires");
    SetTargetFPS(60);

    OrbitalCamera camera;
    camera.distance = fmaxf(grid.cols, grid.rows) * (grid.dims.width + grid.dims.spacing) * 1.5f;

    RenderOptions renderOpt;
    // Synchronise taille/hauteur du cube avec les dims physiques
    renderOpt.cubeSize   = grid.dims.width;
    renderOpt.cubeHeight = grid.dims.height * 0.1f; // échelle visuelle

    DimsPanel dimsPanel;
    float lightAngle = 45.0f;

    // --- Boucle ---
    while (!WindowShouldClose()) {

        camera.update();
        updateRenderOptions(renderOpt);

        // Panneau dimensions : géré dans la section dessin

        // RT toggle
        if (IsKeyPressed(KEY_V) && rtAvailable) {
            rtEnabled = !rtEnabled;
            std::cout << "[RT] " << (rtEnabled ? "ON" : "OFF") << "\n";
        }

        // Recharger CSV
        if (IsKeyPressed(KEY_R))
            ThermalModel::chargerCSV(csvPath, grid);

        // Lumière
        bool lightMoved = IsKeyDown(KEY_J) || IsKeyDown(KEY_K);
        if (IsKeyDown(KEY_J)) lightAngle -= 1.0f;
        if (IsKeyDown(KEY_K)) lightAngle += 1.0f;
        if (lightMoved && rtAvailable) {
            float rad = lightAngle * DEG2RAD;
            shadowCompute.setLightDir(cosf(rad), 0.3f, sinf(rad), lightParams);
            shadowCompute.compute(lightParams);
        }
        if (IsKeyPressed(KEY_N) && rtAvailable) {
            lightParams.numSamples = (int)fmax(1,  lightParams.numSamples - 1);
            shadowCompute.compute(lightParams);
        }
        if (IsKeyPressed(KEY_M) && rtAvailable) {
            lightParams.numSamples = (int)fmin(32, lightParams.numSamples + 1);
            shadowCompute.compute(lightParams);
        }

        // --- Rendu ---
        BeginDrawing();
        ClearBackground({25, 25, 30, 255});

        BeginMode3D(camera.get());
            if (rtEnabled)
                drawGrid3DWithShadows(grid, renderOpt,
                                      shadowCompute.shadowFactors, true);
            else
                drawGrid3D(grid, renderOpt);
        EndMode3D();

        drawHUD(SW, SH, params, grid, renderOpt);
        drawRTOverlay(SW, SH, rtAvailable, rtEnabled, lightParams, lightAngle);
        if (dimsPanel.update(grid.dims, SW)) {
            grid.rebuildPositions();
            renderOpt.cubeSize   = grid.dims.width;
            renderOpt.cubeHeight = grid.dims.height * 0.1f;
            camera.distance = fmaxf(grid.cols, grid.rows)
                            * (grid.dims.width + grid.dims.spacing) * 1.5f;
        }

        // Aide touche P
        DrawText("[P] Dimensions assemblage",
                 SW - 210, SH - 40, 12, {100,100,100,255});

        EndDrawing();
    }

    shadowCompute.cleanup();
    vkCtx.cleanup();
    CloseWindow();
    return 0;
}
