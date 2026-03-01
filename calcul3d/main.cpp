// ============================================================
//  main.cpp — Visualiseur + Simulation Thermique Nucléaire
// ============================================================
#include <raylib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>

#include "core/ReactorParams.hpp"
#include "core/GridData.hpp"
#include "core/AssemblageLoader.hpp"
#include "render/Camera.hpp"
#include "render/ColorMap.hpp"
#include "render/CubeRenderer.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/HUD.hpp"
#include "render/DimsPanel.hpp"
#include "render/SimPanel.hpp"
#include "render/HeatmapPanel.hpp"
#include "physics/ThermalModel.hpp"
#include "physics/NeutronFlux.hpp"
#include "compute/VulkanContext.hpp"
#include "compute/ShadowCompute.hpp"
#include "compute/ThermalCompute.hpp"

// ============================================================
//  Overlay RT
// ============================================================
inline void drawRTOverlay(int sw, bool rtAvailable, bool rtEnabled,
                           const LightParams& lp, float lightAngle) {
    int x = 10, y = 170;
    DrawRectangle(x, y, 230, 90, {0,0,0,170});
    DrawRectangleLines(x, y, 230, 90, {80,80,80,255});
    DrawText("RAY TRACING (Vulkan)", x+8, y+8, 13, LIGHTGRAY);
    if (!rtAvailable) { DrawText("Non disponible", x+8, y+28, 12, {180,80,80,255}); return; }
    Color sc = rtEnabled ? Color{100,255,100,255} : Color{180,80,80,255};
    DrawText(rtEnabled ? "ON  [V]" : "OFF [V]", x+8, y+28, 12, sc);
    char buf[64];
    snprintf(buf,sizeof(buf),"Samples : %d  [N/B]", lp.numSamples); // N/B au lieu de N/M
    DrawText(buf, x+8, y+46, 12, LIGHTGRAY);
    snprintf(buf,sizeof(buf),"Lumiere : %.0f deg  [J/K]", lightAngle);
    DrawText(buf, x+8, y+64, 12, {255,220,100,255});
}

// ============================================================
//  Overlay convergence
// ============================================================
inline void drawConvergenceOverlay(int sw, int sh,
                                    const SimControl& ctrl,
                                    float maxDeltaT,
                                    float threshold,
                                    bool converged) {
    int x = 10, y = 270;
    DrawRectangle(x, y, 230, 80, {0,0,0,170});
    DrawRectangleLines(x, y, 230, 80, {80,80,80,255});
    DrawText("SIMULATION", x+8, y+8, 13, LIGHTGRAY);

    char buf[64];
    snprintf(buf,sizeof(buf),"Temps : %.1f s", ctrl.simTime);
    DrawText(buf, x+8, y+26, 12, LIGHTGRAY);

    snprintf(buf,sizeof(buf),"dT max : %.4f C", maxDeltaT);
    Color dtCol = (maxDeltaT < threshold*10) ? Color{100,255,100,255} : LIGHTGRAY;
    DrawText(buf, x+8, y+42, 12, dtCol);

    if (converged) {
        DrawText("CONVERGE !", x+8, y+58, 13, {100,255,100,255});
    } else {
        const char* stateStr =
            ctrl.state == SimState::RUNNING ? "En cours..." :
            ctrl.state == SimState::PAUSED  ? "En pause"   : "Arrete";
        Color sc = ctrl.state == SimState::RUNNING ? Color{255,200,50,255} : LIGHTGRAY;
        DrawText(stateStr, x+8, y+58, 12, sc);
    }
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
            if (!raw[r][c].isAssembly) continue;
            Cube cube;
            cube.sym         = raw[r][c].symbol;
            cube.col         = symbolColor(cube.sym);
            cube.temperature = params.tempEntree;
            cube.flux        = 0.0f;
            cube.row         = r;
            cube.col_idx     = c;
            cube.pos         = {0,0,0};
            grid.cubes.push_back(cube);
        }
    grid.rebuildPositions();
    std::cout << "[Main] " << grid.cubes.size() << " assemblages\n";

    // --- Vulkan ---
    VulkanContext    vkCtx;
    ShadowCompute    shadowCompute;
    ThermalCompute   thermalCompute;
    LightParams      lightParams;
    bool rtAvailable      = false;
    bool thermalAvailable = false;

    if (vkCtx.init()) {
        if (shadowCompute.init(vkCtx, grid)) {
            rtAvailable = true;
            shadowCompute.compute(lightParams);
            std::cout << "[Main] Vulkan RT OK\n";
        }
        if (thermalCompute.init(vkCtx, grid, params.tempEntree)) {
            thermalAvailable = true;
            std::cout << "[Main] Vulkan Compute thermique OK\n";
        }
    }
    bool rtEnabled = rtAvailable;

    // --- Flux neutronique ---
    NeutronFlux neutronFlux;
    if (thermalAvailable)
        neutronFlux.init(thermalCompute.params.dt,
                         thermalCompute.params.rho_cp, 2.0f);

    std::vector<float> q_vol_flat(grid.rows * grid.cols, 0.0f);
    bool fluxDirty = true;

    auto recalcFlux = [&](FluxMode mode) {
        neutronFlux.mode = mode;
        std::vector<float> temps(grid.cubes.size());
        for (int i=0; i<(int)grid.cubes.size(); ++i)
            temps[i] = grid.cubes[i].temperature;
        auto q_cubes = neutronFlux.calculer(grid, temps);
        std::fill(q_vol_flat.begin(), q_vol_flat.end(), 0.0f);
        for (int i=0; i<(int)grid.cubes.size(); ++i) {
            int idx = grid.cubes[i].row * grid.cols + grid.cubes[i].col_idx;
            if (idx < (int)q_vol_flat.size())
                q_vol_flat[idx] = q_cubes[i];
        }
        fluxDirty = false;
    };
    recalcFlux(FluxMode::COSINUS_REP);

    // --- Raylib ---
    const int SW = 1280, SH = 800;
    InitWindow(SW, SH, "Visualiseur Thermique Nucleaire");
    SetTargetFPS(60);

    OrbitalCamera camera;
    camera.distance = fmaxf(grid.cols, grid.rows)
                    * (grid.dims.width + grid.dims.spacing) * 1.5f;

    RenderOptions renderOpt;
    renderOpt.cubeSize     = grid.dims.width;
    renderOpt.cubeHeight   = grid.dims.height * 0.1f;
    renderOpt.colormapMode = true;

    DimsPanel    dimsPanel;
    SimPanel     simPanel;
    HeatmapPanel heatmapPanel;
    SimControl   simCtrl;

    float lightAngle = 45.0f;
    float maxDeltaT  = 999.0f;
    float convergenceThreshold = 0.05f;
    bool  converged  = false;
    bool  needReset  = false;

    std::vector<float> T_prev(grid.cubes.size(), params.tempEntree);

    // --- Boucle ---
    while (!WindowShouldClose()) {

        camera.update();
        updateRenderOptions(renderOpt);

        // RT — samples déplacés sur N/B pour libérer M
        if (IsKeyPressed(KEY_V) && rtAvailable) rtEnabled = !rtEnabled;
        if (IsKeyDown(KEY_J)) lightAngle -= 1.0f;
        if (IsKeyDown(KEY_K)) lightAngle += 1.0f;
        if ((IsKeyDown(KEY_J)||IsKeyDown(KEY_K)) && rtAvailable) {
            float rad = lightAngle * DEG2RAD;
            shadowCompute.setLightDir(cosf(rad), 0.3f, sinf(rad), lightParams);
            shadowCompute.compute(lightParams);
        }
        if (IsKeyPressed(KEY_N) && rtAvailable) {
            lightParams.numSamples = (int)fmax(1, lightParams.numSamples-1);
            shadowCompute.compute(lightParams);
        }
        if (IsKeyPressed(KEY_B) && rtAvailable) {  // B au lieu de M
            lightParams.numSamples = (int)fmin(32, lightParams.numSamples+1);
            shadowCompute.compute(lightParams);
        }

        // --- Simulation thermique ---
        if (thermalAvailable && simCtrl.state == SimState::RUNNING && !converged) {

            // Ralenti : sauter des frames si frameSkipMax > 0
            bool doStep = true;
            if (simCtrl.frameSkipMax > 0) {
                simCtrl.frameSkip++;
                doStep = (simCtrl.frameSkip > simCtrl.frameSkipMax);
                if (doStep) simCtrl.frameSkip = 0;
            }

            if (doStep) {
                for (int i=0; i<(int)grid.cubes.size(); ++i)
                    T_prev[i] = grid.cubes[i].temperature;

                recalcFlux(simCtrl.fluxMode);

                thermalCompute.step(simCtrl.stepsPerFrame, q_vol_flat);
                thermalCompute.applyToGrid(grid);

                maxDeltaT = 0.0f;
                for (int i=0; i<(int)grid.cubes.size(); ++i)
                    maxDeltaT = fmaxf(maxDeltaT,
                        fabsf(grid.cubes[i].temperature - T_prev[i]));

                simCtrl.simTime += thermalCompute.params.dt * simCtrl.stepsPerFrame;
                simCtrl.T_min = grid.tempMin;
                simCtrl.T_max = grid.tempMax;

                if (maxDeltaT < convergenceThreshold) {
                    converged = true;
                    simCtrl.state = SimState::PAUSED;
                    std::cout << "[Sim] Convergence ! t=" << simCtrl.simTime
                              << " s  T_max=" << grid.tempMax << " C\n";
                    for (auto& c : grid.cubes)
                        std::cout << "  [" << c.row << "," << c.col_idx
                                  << "] " << c.temperature << " C\n";
                }
            }
        }

        // Reset — one-shot via needReset
        if (simCtrl.state == SimState::STOPPED && !needReset)
            needReset = true;
        if (needReset && simCtrl.state == SimState::STOPPED && thermalAvailable) {
            needReset = false;
            thermalCompute.reset(params.tempEntree);
            thermalCompute.applyToGrid(grid);
            for (auto& c : grid.cubes) c.temperature = params.tempEntree;
            simCtrl.simTime   = 0.0f;
            simCtrl.T_min     = params.tempEntree;
            simCtrl.T_max     = params.tempEntree;
            maxDeltaT         = 999.0f;
            converged         = false;
            simCtrl.frameSkip = 0;
            recalcFlux(simCtrl.fluxMode);
        }

        // --- Rendu ---
        BeginDrawing();
        ClearBackground({25,25,30,255});

        BeginMode3D(camera.get());
            if (rtEnabled)
                drawGrid3DWithShadows(grid, renderOpt,
                                      shadowCompute.shadowFactors, true);
            else
                drawGrid3D(grid, renderOpt);
        EndMode3D();

        drawHUD(SW, SH, params, grid, renderOpt);
        drawRTOverlay(SW, rtAvailable, rtEnabled, lightParams, lightAngle);
        drawConvergenceOverlay(SW, SH, simCtrl, maxDeltaT,
                               convergenceThreshold, converged);

        if (dimsPanel.update(grid.dims, SW)) {
            grid.rebuildPositions();
            renderOpt.cubeSize   = grid.dims.width;
            renderOpt.cubeHeight = grid.dims.height * 0.1f;
        }

        bool simChanged = simPanel.update(simCtrl, SW, SH);
        if (simChanged) fluxDirty = true;
        if (fluxDirty) recalcFlux(simCtrl.fluxMode);

        heatmapPanel.draw(grid, SW, SH);

        if (converged) {
            char buf[64];
            snprintf(buf, sizeof(buf),
                     "CONVERGE a t=%.0f s  T_max=%.1f C",
                     simCtrl.simTime, simCtrl.T_max);
            int tw = MeasureText(buf, 16);
            DrawRectangle(SW/2-tw/2-10, SH/2-20, tw+20, 34, {0,0,0,200});
            DrawText(buf, SW/2-tw/2, SH/2-12, 16, {100,255,100,255});
        }

        DrawText("[S]Sim [P/M]Vitesse [H]Heatmap [T]Colormap [V]RT",
                 10, SH-20, 11, {80,80,80,255});

        EndDrawing();
    }

    shadowCompute.cleanup();
    thermalCompute.cleanup();
    vkCtx.cleanup();
    CloseWindow();
    return 0;
}