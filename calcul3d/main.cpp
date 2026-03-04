// ============================================================
//  main.cpp — Visualiseur + Simulation Thermique + Neutronique
//  Corrections Phase 1 :
//    [Bug1] neutronCompute.init() dans le bloc vkCtx
//    [Bug3] ReactorConfig synced depuis ReactorParams console
//    [Bug4] puissance appliquee a phi_eff
//    [Bug5] moderateur passe a rebuildXS
//    [Bug6] fluxDirty double-recalc evite
//    [Bug7] tempSortie utilisee pour normaliser puissance
//    [Bug8] autoGenerateZones rappele avant reinit neutronique
//    [BugConv] converged reset sur slicesChanged et reset propre
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
#include "render/HUD.hpp"
#include "render/DimsPanel.hpp"
#include "render/SimPanel.hpp"
#include "render/HeatmapPanel.hpp"
#include "render/CubeRenderer3D.hpp"
#include "render/CoolantPanel.hpp"
#include "render/ModeratorRenderer.hpp"
#include "render/ReactorPanel.hpp"
#include "physics/ThermalModel.hpp"
#include "physics/NeutronFlux.hpp"
#include "physics/CoolantModel.hpp"
#include "physics/NeutronCrossSection.hpp"
#include "compute/VulkanContext.hpp"
#include "compute/ThermalCompute.hpp"
#include "compute/NeutronCompute.hpp"

// ============================================================
//  Overlay convergence
// ============================================================
inline void drawConvergenceOverlay(int sw, int sh,
                                    const SimControl& ctrl,
                                    float maxDeltaT,
                                    float threshold,
                                    bool converged,
                                    float k_eff     = 1.0f,
                                    bool neutronOK  = false)
{
    int x = 10, y = 270;
    int h = neutronOK ? 112 : 82;
    DrawRectangle(x, y, 230, h, {0,0,0,170});
    DrawRectangleLines(x, y, 230, h, {80,80,80,255});
    DrawText("SIMULATION", x+8, y+8, 13, LIGHTGRAY);

    char buf[64];
    snprintf(buf, sizeof(buf), "Temps : %.1f s", ctrl.simTime);
    DrawText(buf, x+8, y+26, 12, LIGHTGRAY);

    snprintf(buf, sizeof(buf), "dT max : %.4f C", maxDeltaT);
    Color dtCol = (maxDeltaT < threshold*10) ? Color{100,255,100,255} : LIGHTGRAY;
    DrawText(buf, x+8, y+42, 12, dtCol);

    if (neutronOK) {
        snprintf(buf, sizeof(buf), "k_eff = %.5f", k_eff);
        Color kCol = (k_eff > 1.005f) ? Color{255,80,80,255}
                   : (k_eff < 0.995f) ? Color{80,160,255,255}
                                       : Color{100,255,100,255};
        DrawText(buf, x+8, y+58, 12, kCol);
        const char* kSt = (k_eff > 1.005f) ? "SURGENERANT"
                        : (k_eff < 0.995f)  ? "SOUS-CRITIQUE" : "CRITIQUE";
        DrawText(kSt, x+8, y+74, 11, kCol);
    }

    int yState = neutronOK ? y+92 : y+60;
    if (converged) {
        DrawText("CONVERGE !", x+8, yState, 13, {100,255,100,255});
    } else {
        const char* stateStr =
            ctrl.state == SimState::RUNNING ? "En cours..." :
            ctrl.state == SimState::PAUSED  ? "En pause"   : "Arrete";
        Color sc = ctrl.state == SimState::RUNNING ? Color{255,200,50,255} : LIGHTGRAY;
        DrawText(stateStr, x+8, yState, 12, sc);
    }
}

// ============================================================
//  MAIN
// ============================================================
int main() {
    const std::string gridPath = "data/Assemblage.txt";

    { std::ifstream t(gridPath); if (!t.is_open()) {
        std::cerr << "Fichier introuvable : " << gridPath << "\n"; return 1; } }

    ReactorParams params = ReactorParams::lireDepuisFichier(gridPath);
    params.saisirConsole();
    std::cout << "Sauvegarder ? (o/n) : ";
    std::string rep; std::getline(std::cin, rep);
    if (rep == "o" || rep == "O") params.sauvegarder(gridPath);

    // ---- Chargement grille ----
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

    // ---- [Bug3] ReactorConfig synchronisé depuis ReactorParams ----
    ReactorConfig     reactorCfg;
    reactorCfg.enrichment  = params.enrichissement / 100.0f; // % -> fraction
    reactorCfg.moderateur  = params.moderateur;
    reactorCfg.puissance   = params.puissance;
    reactorCfg.autoEnrich  = false;

    ReactorPanel      reactorPanel;
    NeutronCompute    neutronCompute;
    ModeratorRenderer modRenderer;
    bool neutronAvailable = false;

    grid.autoGenerateZones(reactorCfg.nReflectorRings);

    SimControl simCtrl;

    VulkanContext  vkCtx;
    ThermalCompute thermalCompute;
    bool thermalAvailable = false;

    // ---- [Bug1] neutronCompute.init() dans le même bloc vkCtx ----
    if (vkCtx.init()) {
        if (thermalCompute.init(vkCtx, grid, params.tempEntree, simCtrl.nSlices)) {
            thermalAvailable = true;
            grid.nSlices     = simCtrl.nSlices;
            simCtrl.dt_current = thermalCompute.params.dt;
            std::cout << "[Main] Vulkan Compute thermique OK\n";
        }
        // Neutronique dans le même bloc Vulkan valide
        neutronAvailable = neutronCompute.init(
            vkCtx, grid,
            reactorCfg.reactorType,
            reactorCfg.enrichment,
            params.tempEntree
        );
        if (neutronAvailable)
            simCtrl.fluxMode = FluxMode::DIFFUSION_2G;
    }

    NeutronFlux neutronFlux;
    if (thermalAvailable)
        neutronFlux.init(thermalCompute.params.dt,
                         thermalCompute.params.rho_cp, 2.0f);

    std::vector<float> q_vol_flat(grid.rows * grid.cols, 0.0f);
    bool fluxDirty = true;

    // ---- recalcFlux : GPU ou fallback cosinus ----
    // [Bug4] puissance appliquée à phi_eff
    // [Bug7] tempSortie comme référence de normalisation puissance
    auto recalcFlux = [&](FluxMode mode) {
        if (mode == FluxMode::DIFFUSION_2G && neutronAvailable) {
            // ── Modèle Diffusion 2 groupes CPU (± GPU) ──────────────────
            float DeltaT_ref = 40.0f;
            float DeltaT_usr = params.tempSortie - params.tempEntree;
            float powerScale = (reactorCfg.puissance / 100.0f)
                             * (DeltaT_usr / DeltaT_ref);
            powerScale = fmaxf(0.01f, powerScale);

            for (int i = 0; i < (int)grid.cubes.size(); ++i) {
                int id = grid.cubes[i].row * grid.cols + grid.cubes[i].col_idx;
                if (id < (int)q_vol_flat.size() && id < neutronCompute.total2d) {
                    float phi = neutronCompute.phi_total[id];
                    q_vol_flat[id] = phi
                        * neutronFlux.sigma_f
                        * neutronFlux.phi0
                        * neutronFlux.E_fission
                        * 1e6f * neutronFlux.scale
                        * powerScale;
                }
            }
        } else {
            // Fallback cosinus
            neutronFlux.mode = mode;
            // [Bug4] scale modifié par puissance
            float savedScale = neutronFlux.scale;
            neutronFlux.scale *= (reactorCfg.puissance / 100.0f);
            std::vector<float> temps(grid.cubes.size());
            for (int i = 0; i < (int)grid.cubes.size(); ++i)
                temps[i] = grid.cubes[i].temperature;
            auto q_cubes = neutronFlux.calculer(grid, temps);
            neutronFlux.scale = savedScale; // restaurer
            std::fill(q_vol_flat.begin(), q_vol_flat.end(), 0.0f);
            for (int i = 0; i < (int)grid.cubes.size(); ++i) {
                int id = grid.cubes[i].row * grid.cols + grid.cubes[i].col_idx;
                if (id < (int)q_vol_flat.size())
                    q_vol_flat[id] = q_cubes[i];
            }
        }
        fluxDirty = false;
    };
    recalcFlux(FluxMode::COSINUS_REP);

    // --- Caloporteur ---
    CoolantModel  coolantModel;
    CoolantPanel  coolantPanel;
    CoolantParams coolantParams;
    coolantParams.T_inlet = params.tempEntree;
    coolantParams.H_coeur = grid.dims.height;
    coolantParams.D_h     = grid.dims.spacing * 0.8f;
    coolantParams.A_canal = coolantParams.D_h * coolantParams.D_h * 0.785f;
    coolantModel.init(grid, coolantParams);

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

    float maxDeltaT           = 999.0f;
    float convergenceThreshold = 0.05f;
    bool  converged            = false;
    bool  needReset            = false;

    std::vector<float> T_prev(grid.cubes.size(), params.tempEntree);

    while (!WindowShouldClose()) {

        camera.update();
        updateRenderOptions(renderOpt);

        // ================================================================
        //  Simulation
        // ================================================================
        if (thermalAvailable && simCtrl.state == SimState::RUNNING && !converged) {

            bool doStep = true;
            if (simCtrl.frameSkipMax > 0) {
                simCtrl.frameSkip++;
                doStep = (simCtrl.frameSkip > simCtrl.frameSkipMax);
                if (doStep) simCtrl.frameSkip = 0;
            }

            if (doStep) {
                for (int i = 0; i < (int)grid.cubes.size(); ++i)
                    T_prev[i] = grid.cubes[i].temperature;

                // Neutronique GPU — [Bug5] moderateur passé à rebuildXS
                if (neutronAvailable) {
                    // CPU 2G (+ GPU si dispo) — reactorCfg.useGPUFlux controle GPU uniquement
                    neutronCompute.rebuildXS(grid, params.tempEntree,
                                            reactorCfg.moderateur);
                    neutronCompute.step(5, 3);
                    neutronCompute.applyToGrid(grid);
                }

                // [Bug6] recalcFlux appelé une seule fois ici (pas en double)
                recalcFlux(simCtrl.fluxMode);

                thermalCompute.step(simCtrl.stepsPerFrame, q_vol_flat);
                thermalCompute.applyToGrid(grid);

                if (coolantPanel.active)
                    coolantModel.update(grid);

                maxDeltaT = 0.0f;
                for (int i = 0; i < (int)grid.cubes.size(); ++i)
                    maxDeltaT = fmaxf(maxDeltaT,
                        fabsf(grid.cubes[i].temperature - T_prev[i]));

                simCtrl.simTime += thermalCompute.params.dt * simCtrl.stepsPerFrame;
                simCtrl.T_min    = grid.tempMin;
                simCtrl.T_max    = grid.tempMax;

                if (maxDeltaT < convergenceThreshold) {
                    converged     = true;
                    simCtrl.state = SimState::PAUSED;
                    float keff    = neutronAvailable ? neutronCompute.k_eff : 1.0f;
                    std::cout << "[Sim] Convergence ! t=" << simCtrl.simTime
                              << " s  T_max=" << grid.tempMax
                              << " C  k_eff=" << keff << "\n";
                    for (auto& c : grid.cubes)
                        std::cout << "  [" << c.row << "," << c.col_idx
                                  << "] " << c.temperature << " C\n";
                }
            }
        }

        // ----------------------------------------------------------------
        //  Reset  [BugConv] converged remis à false, maxDeltaT=999
        // ----------------------------------------------------------------
        if (simCtrl.state == SimState::STOPPED && !needReset) needReset = true;
        if (needReset && simCtrl.state == SimState::STOPPED && thermalAvailable) {
            needReset = false;
            thermalCompute.reset(params.tempEntree);
            thermalCompute.applyToGrid(grid);
            for (auto& c : grid.cubes) c.temperature = params.tempEntree;
            simCtrl.simTime   = 0.0f;
            simCtrl.T_min     = params.tempEntree;
            simCtrl.T_max     = params.tempEntree;
            maxDeltaT         = 999.0f;   // [BugConv] reset explicite
            converged         = false;    // [BugConv]
            simCtrl.frameSkip = 0;
            coolantModel.init(grid, coolantParams);
            // [Bug8] autoGenerateZones avant reinit neutronique
            if (neutronAvailable) {
                grid.autoGenerateZones(reactorCfg.nReflectorRings); // [Bug8]
                neutronCompute.cleanup();
                neutronAvailable = neutronCompute.init(
                    vkCtx, grid, reactorCfg.reactorType,
                    reactorCfg.enrichment, params.tempEntree);
                if (neutronAvailable)
                    simCtrl.fluxMode = FluxMode::DIFFUSION_2G;
            }
            recalcFlux(simCtrl.fluxMode);
        }

        // ----------------------------------------------------------------
        //  Changement de tranches [BugConv] converged reset ici aussi
        // ----------------------------------------------------------------
        if (simCtrl.slicesChanged && thermalAvailable) {
            simCtrl.slicesChanged = false;
            thermalCompute.reinit(vkCtx, grid, params.tempEntree, simCtrl.nSlices);
            grid.nSlices           = simCtrl.nSlices;
            simCtrl.dt_current     = thermalCompute.params.dt;
            thermalCompute.applyToGrid(grid);
            converged              = false;  // [BugConv] reset convergence
            maxDeltaT              = 999.0f; // [BugConv]
            simCtrl.simTime        = 0.0f;
        }

        // ----------------------------------------------------------------
        //  Panneau réacteur [R]
        //  [Bug3] on recopie les sliders vers ReactorParams si changed
        // ----------------------------------------------------------------
        reactorPanel.update(reactorCfg,
            neutronAvailable ? neutronCompute.k_eff : 1.0f, simCtrl, SW, SH);

        if (reactorCfg.changed) {
            reactorCfg.changed = false;
            // Sync retour vers ReactorParams (puissance, moderateur, enrichissement)
            params.enrichissement = reactorCfg.enrichment  * 100.0f;
            params.moderateur     = reactorCfg.moderateur;
            params.puissance      = reactorCfg.puissance;
            // Reinit zones + neutronique
            grid.autoGenerateZones(reactorCfg.nReflectorRings);
            if (neutronAvailable) neutronCompute.cleanup();
            neutronAvailable = neutronCompute.init(
                vkCtx, grid, reactorCfg.reactorType,
                reactorCfg.enrichment, params.tempEntree);
            converged     = false;
            maxDeltaT     = 999.0f;
            simCtrl.simTime = 0.0f;
            fluxDirty     = true;
        }

        // ================================================================
        //  Rendu
        // ================================================================
        BeginDrawing();
        ClearBackground({25,25,30,255});

        BeginMode3D(camera.get());
            drawGrid3DAxial(grid, renderOpt);
            modRenderer.showModerator  = reactorCfg.showModerator;
            modRenderer.showReflector  = reactorCfg.showReflector;
            modRenderer.showControlRod = reactorCfg.showControlRod;
            modRenderer.draw(grid, renderOpt, (float)GetTime());
            coolantPanel.draw3DArrows(grid, coolantModel, renderOpt);
        EndMode3D();

        drawHUD(SW, SH, params, grid, renderOpt);
        drawConvergenceOverlay(SW, SH, simCtrl, maxDeltaT,
                               convergenceThreshold, converged,
                               neutronAvailable ? neutronCompute.k_eff : 1.0f,
                               neutronAvailable);

        if (dimsPanel.update(grid.dims, SW)) {
            grid.rebuildPositions();
            renderOpt.cubeSize   = grid.dims.width;
            renderOpt.cubeHeight = grid.dims.height * 0.1f;
        }

        // [Bug6] fluxDirty seulement si simulation n'est pas en cours
        bool simChanged = simPanel.update(simCtrl, SW, SH, neutronAvailable,
                                                   neutronAvailable && neutronCompute.gpuAccel);
        if (simChanged) fluxDirty = true;
        if (fluxDirty && simCtrl.state != SimState::RUNNING)
            recalcFlux(simCtrl.fluxMode);  // [Bug6] pas en double

        if (IsKeyPressed(KEY_F))
            coolantPanel.displayMode = (CoolantDisplayMode)(
                ((int)coolantPanel.displayMode + 1) % 3);

        if (coolantPanel.updatePanel(coolantParams, simCtrl, SW, SH))
            coolantModel.init(grid, coolantParams);

        coolantPanel.draw2DOverlay(grid, coolantModel, SW, SH);
        coolantPanel.drawInfoOverlay(coolantModel, SW);

        heatmapPanel.draw(grid, SW, SH);

        if (converged) {
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "CONVERGE a t=%.0f s  T_max=%.1f C  [%s]",
                     simCtrl.simTime, simCtrl.T_max,
                     simCtrl.fluxMode == FluxMode::DIFFUSION_2G ? "Diffusion 2G"
                   : simCtrl.fluxMode == FluxMode::COSINUS_REP  ? "Cosinus REP"
                   :                                               "Uniforme");
            int tw = MeasureText(buf, 16);
            // Bandeau en bas au centre, au-dessus de la barre de touches
            DrawRectangle(SW/2-tw/2-10, SH-42, tw+20, 22, {0,80,0,220});
            DrawText(buf, SW/2-tw/2, SH-38, 14, {150,255,150,255});
        }

        DrawText(
            "[S]Sim [C]Caloporteur [F]Fluide [H]Heatmap [T]Colormap [G]Grille [W]Fils [R]Reacteur",
            10, SH-20, 11, {80,80,80,255});

        // Guide démarrage GPU : affiché les 12 premières secondes
        if (!converged && GetTime() < 12.0 && simCtrl.state == SimState::STOPPED) {
            int gx = SW/2 - 230, gy = SH/2 - 80;
            DrawRectangle(gx, gy, 460, 130, {5,15,30,230});
            DrawRectangleLines(gx, gy, 460, 130, {80,200,80,255});
            DrawText("DEMARRAGE SIMULATION GPU", gx+10, gy+8, 14, {100,255,100,255});
            DrawText("1. [R]  Choisir technologie + enrichissement",  gx+10, gy+32, 12, LIGHTGRAY);
            DrawText("        Cliquer  Appliquer",                    gx+10, gy+48, 12, {100,220,100,255});
            DrawText("2. [C]  Choisir caloporteur + activer",         gx+10, gy+64, 12, LIGHTGRAY);
            DrawText("3. [S]  Start  (Diffusion 2G CPU actif automatiquement)",  gx+10, gy+80, 12, LIGHTGRAY);
            DrawText("4. [H]  Plan de coupe 2D thermique",            gx+10, gy+96, 12, LIGHTGRAY);
            char tb[32]; snprintf(tb,sizeof(tb),"(disparait dans %.0fs)", 12.0-GetTime());
            DrawText(tb, gx+300, gy+112, 10, {80,80,80,255});
        }

        EndDrawing();
    }

    if (neutronAvailable) neutronCompute.cleanup();
    thermalCompute.cleanup();
    vkCtx.cleanup();
    CloseWindow();
    return 0;
}