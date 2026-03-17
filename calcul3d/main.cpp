// ============================================================
//  main.cpp  v2.0
//
//  NOUVEAUTÉS v2 :
//    [Mesh1]  Maillage piloté par h_target (MeshConfig)
//             → cols/rows/slices calculés depuis dimensions physiques
//             → cells ne se déforment plus si les dims changent
//    [Mesh2]  DimsPanel v2 (touche D) : saisie dimensions + h_target
//             Affiche cols×rows×slices + rapport d'aspect + warning
//    [GPU1]   NeutronCompute v2 : XS SoA, neutron_fvm.comp, précurseurs GPU
//    [GPU2]   VulkanContext v2 : uploadToDeviceLocal, createComputePipeline
//
//  Compatibilité maintenue avec :
//    ThermalCompute (init/reinit/reset/cleanup inchangés)
//    ReactorPanel, SimPanel, CoolantPanel (API inchangée)
//    FluxMode::DIFFUSION_2G / COSINUS_REP / UNIFORME
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
#include "render/DimsPanel.hpp"        // ← [P] dimensions physiques
#include "render/MeshPanel.hpp"          // ← [D] résolution maillage
#include "render/SimPanel.hpp"
#include "render/HeatmapPanel.hpp"
#include "render/CubeRenderer3D.hpp"
#include "render/CoolantPanel.hpp"
#include "render/ModeratorRenderer.hpp"
#include "render/ReactorPanel.hpp"
#include "render/XenonPanel.hpp"
#include "physics/ThermalModel.hpp"
#include "physics/NeutronFlux.hpp"
#include "physics/CoolantModel.hpp"
#include "physics/NeutronCrossSection.hpp"
#include "compute/VulkanContext.hpp"   // ← v2 (uploadToDeviceLocal)
#include "compute/ThermalCompute.hpp"
#include "compute/NeutronCompute.hpp"  // ← v2 (SoA + FVM)

// ============================================================
//  Overlay simulation
// ============================================================
inline void drawConvergenceOverlay(int sw, int sh,
                                    const SimControl& ctrl,
                                    float maxDeltaT,
                                    float threshold,
                                    bool  /*unused*/,
                                    float k_eff     = 1.0f,
                                    bool  neutronOK = false)
{
    int x = 10, y = 270;
    int h = neutronOK ? 112 : 82;
    DrawRectangle(x, y, 230, h, {0,0,0,170});
    DrawRectangleLines(x, y, 230, h, {80,80,80,255});
    DrawText("SIMULATION TRANSITOIRE", x+8, y+8, 11, LIGHTGRAY);

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
    const char* stateStr =
        ctrl.state == SimState::RUNNING ? "En cours..." :
        ctrl.state == SimState::PAUSED  ? "En pause"   : "Arrete";
    Color sc = ctrl.state == SimState::RUNNING ? Color{255,200,50,255} : LIGHTGRAY;
    DrawText(stateStr, x+8, yState, 12, sc);
}

// ============================================================
//  Helper : (ré)initialise neutronique + thermique après
//  changement de maillage ou de paramètres réacteur
// ============================================================
static void reinitSimulation(
    VulkanContext&    vkCtx,
    GridData&         grid,
    MeshConfig&       mc,
    ThermalCompute&   tc,
    NeutronCompute&   nc,
    NeutronFlux&      nf,
    SimControl&       ctrl,
    ReactorConfig&    rcfg,
    CoolantModel&     cm,
    CoolantParams&    cparams,
    float             T0,
    bool&             thermalAvail,
    bool&             neutronAvail)
{
    ctrl.state    = SimState::STOPPED;
    ctrl.simTime  = 0.0f;

    // Appliquer la géométrie mise à jour
    grid.applyMesh(mc);
    grid.rebuildPositions();
    grid.autoGenerateZones(rcfg.nReflectorRings);

    // --- Thermique ---
    if (thermalAvail) tc.cleanup();
    thermalAvail = tc.init(vkCtx, grid, T0, mc.slices);
    if (thermalAvail) {
        grid.slices       = mc.slices;
        ctrl.dt_current    = tc.params.dt;
        ctrl.nSlices       = mc.slices;
    }

    // --- Neutronique ---
    neutronAvail = nc.init(vkCtx, grid,
                           rcfg.reactorType,
                           rcfg.enrichment,
                           T0);
    if (neutronAvail)
        ctrl.fluxMode = FluxMode::DIFFUSION_2G;

    // Resync échelle flux cosinus
    if (thermalAvail)
        nf.init(tc.params.dt, tc.params.rho_cp, 2.0f);

    // Resync caloporteur avec nouvelles dimensions physiques
    cparams.H_coeur = mc.assy_height_m;
    cparams.D_h     = mc.assy_gap_m;  // diamètre hydraulique ≈ jeu inter-assy
    cparams.A_canal = cparams.D_h * cparams.D_h * 0.785f;
    cm.init(grid, cparams);

    std::cout << "[reinit] Maillage " << mc.cols << "x"
              << mc.rows << "x" << mc.slices
              << "  sub_xy=" << mc.sub_xy
              << "  sub_z="  << mc.sub_z
              << "  neutron=" << neutronAvail
              << "  thermal=" << thermalAvail << "\n";
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

    // ── Chargement grille ────────────────────────────────────
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
    std::cout << "[Main] " << grid.cubes.size() << " assemblages\n";

    // ── [Mesh1] Initialisation MeshConfig ────────────────────
    MeshConfig meshConfig;
    {
        // Dimensions par défaut REP 900MW (17×17, pitch=0.217m)
        // On utilise les dims du fichier si elles sont plausibles,
        // sinon on tombe sur les valeurs standards REP 900MW
        float file_width   = grid.dims.width;
        float file_spacing = grid.dims.spacing;
        float file_height  = grid.dims.height;
        // Valider : width doit être entre 0.05m et 1.0m
        meshConfig.assy_width_m  = (file_width  > 0.05f && file_width  < 1.0f)
                                 ? file_width  : 0.2140f;
        meshConfig.assy_gap_m    = (file_spacing> 0.001f&& file_spacing< 0.5f)
                                 ? file_spacing: 0.0030f;
        meshConfig.assy_height_m = (file_height > 0.1f  && file_height < 20.0f)
                                 ? file_height : 3.658f;
        meshConfig.n_assy_cols   = grid.cols;
        meshConfig.n_assy_rows   = grid.rows;
        // Par défaut : 8×8 subdivisions par assemblage en XZ
        // sub_z calculé pour avoir des cubes aussi réguliers que possible
        meshConfig.sub_xy = 1;    // 1 cellule par assemblage en XZ
        meshConfig.sub_z  = 16;   // 16 tranches axiales
        meshConfig.update();

        grid.applyMesh(meshConfig);
        grid.rebuildPositions();
    }

    // ── ReactorConfig synchronisé depuis ReactorParams ───────
    ReactorConfig  reactorCfg;
    reactorCfg.enrichment  = params.enrichissement / 100.0f;
    reactorCfg.moderateur  = params.moderateur;
    reactorCfg.puissance   = params.puissance;
    reactorCfg.autoEnrich  = false;

    ReactorPanel      reactorPanel;
    ModeratorRenderer modRenderer;

    grid.autoGenerateZones(reactorCfg.nReflectorRings);

    SimControl simCtrl;

    VulkanContext  vkCtx;
    ThermalCompute thermalCompute;
    NeutronCompute neutronCompute;
    bool thermalAvailable = false;
    bool neutronAvailable = false;

    // ── Init Vulkan ───────────────────────────────────────────
    bool vkOK = true;
    try { vkCtx.init(); }
    catch (const std::exception& e) {
        vkOK = false;
        std::cerr << "[Main] Vulkan init échoué : " << e.what() << "\n";
    }

    if (vkOK) {
        thermalAvailable = thermalCompute.init(vkCtx, grid,
                                               params.tempEntree,
                                               meshConfig.slices);
        if (thermalAvailable) {
            grid.slices       = meshConfig.slices;
            simCtrl.dt_current = thermalCompute.params.dt;
            simCtrl.nSlices    = meshConfig.slices;
            std::cout << "[Main] Vulkan thermique OK\n";
        }

        neutronAvailable = neutronCompute.init(
            vkCtx, grid,
            reactorCfg.reactorType,
            reactorCfg.enrichment,
            params.tempEntree);
        if (neutronAvailable)
            simCtrl.fluxMode = FluxMode::DIFFUSION_2G;
    }

    NeutronFlux neutronFlux;
    if (thermalAvailable)
        neutronFlux.init(thermalCompute.params.dt,
                         thermalCompute.params.rho_cp, 2.0f);

    std::vector<float> q_vol_flat(grid.rows * grid.cols, 0.0f);
    bool fluxDirty = true;

    // ── recalcFlux ───────────────────────────────────────────
    auto recalcFlux = [&](FluxMode mode) {
        if (mode == FluxMode::DIFFUSION_2G && neutronAvailable) {
            float DeltaT_ref = 40.0f;
            float DeltaT_usr = params.tempSortie - params.tempEntree;
            float powerScale = (reactorCfg.puissance / 100.0f)
                             * (DeltaT_usr / DeltaT_ref);
            powerScale = fmaxf(0.01f, powerScale);

            int N2d = grid.rows * grid.cols;
            if ((int)q_vol_flat.size() != N2d)
                q_vol_flat.assign(N2d, 0.0f);

            for (int i = 0; i < (int)grid.cubes.size(); ++i) {
                int id = grid.cubes[i].row * grid.cols + grid.cubes[i].col_idx;
                if (id < N2d && id < neutronCompute.total2d) {
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
            neutronFlux.mode = mode;
            float savedScale = neutronFlux.scale;
            neutronFlux.scale *= (reactorCfg.puissance / 100.0f);
            std::vector<float> temps(grid.cubes.size());
            for (int i = 0; i < (int)grid.cubes.size(); ++i)
                temps[i] = grid.cubes[i].temperature;
            auto q_cubes = neutronFlux.calculer(grid, temps);
            neutronFlux.scale = savedScale;

            int N2d = grid.rows * grid.cols;
            if ((int)q_vol_flat.size() != N2d)
                q_vol_flat.assign(N2d, 0.0f);
            std::fill(q_vol_flat.begin(), q_vol_flat.end(), 0.0f);
            for (int i = 0; i < (int)grid.cubes.size(); ++i) {
                int id = grid.cubes[i].row * grid.cols + grid.cubes[i].col_idx;
                if (id < N2d) q_vol_flat[id] = q_cubes[i];
            }
        }
        fluxDirty = false;
    };
    recalcFlux(FluxMode::COSINUS_REP);

    // ── Caloporteur ──────────────────────────────────────────
    CoolantModel  coolantModel;
    CoolantPanel  coolantPanel;
    CoolantParams coolantParams;
    coolantParams.T_inlet = params.tempEntree;
    coolantParams.H_coeur = grid.dims.height;
    coolantParams.D_h     = grid.dims.spacing * 0.8f;
    coolantParams.A_canal = coolantParams.D_h * coolantParams.D_h * 0.785f;
    coolantModel.init(grid, coolantParams, meshConfig.n_assy_cols, meshConfig.n_assy_rows);

    // ── Fenêtre ───────────────────────────────────────────────
    const int SW = 1280, SH = 800;
    InitWindow(SW, SH, "Simulateur Neutronique-Thermique v2");
    SetTargetFPS(60);

    OrbitalCamera camera;
    camera.distance = fmaxf((float)meshConfig.n_assy_cols,
                               (float)meshConfig.n_assy_rows)
                    * meshConfig.assy_pitch_m * 1.5f;

    RenderOptions renderOpt;
    // cubeSize = facteur d echelle [0..1] × assy_pitch
    renderOpt.cubeSize   = 0.95f;
    renderOpt.cubeHeight = meshConfig.core_height_m() * 0.1f;
    renderOpt.colormapMode = true;

    // ── [Mesh2] DimsPanel v2 ─────────────────────────────────
    // transientHistory déclaré ICI pour être accessible dans le lambda onChange
    std::vector<TransientPoint> transientHistory;
    transientHistory.reserve(10000);
    float _lastRecordTime = -1.0f;
    const float recordInterval = 1.0f;

    DimsPanel dimsPanel;
    dimsPanel.init(meshConfig);
    MeshPanel meshPanel;
    meshPanel.init(meshConfig);
    dimsPanel.onChange = [&](const MeshConfig& mc) {
        meshConfig = mc;
        reinitSimulation(vkCtx, grid, meshConfig,
                         thermalCompute, neutronCompute,
                         neutronFlux, simCtrl,
                         reactorCfg, coolantModel, coolantParams,
                         params.tempEntree,
                         thermalAvailable, neutronAvailable);
        // Sync rendering
        renderOpt.cubeSize   = 0.95f;
        renderOpt.cubeHeight = meshConfig.core_height_m() * 0.1f;
        camera.distance = fmaxf((float)meshConfig.n_assy_cols,
                                (float)meshConfig.n_assy_rows)
                        * meshConfig.assy_pitch_m * 1.5f;
        q_vol_flat.assign(grid.rows * grid.cols, 0.0f);
        fluxDirty = true;
        transientHistory.clear();
        _lastRecordTime = -1.0f;
    };

    meshPanel.onChange = [&](const MeshConfig& mc) {
        meshConfig = mc;
        reinitSimulation(vkCtx, grid, meshConfig,
                         thermalCompute, neutronCompute,
                         neutronFlux, simCtrl,
                         reactorCfg, coolantModel, coolantParams,
                         params.tempEntree,
                         thermalAvailable, neutronAvailable);
        renderOpt.cubeHeight = meshConfig.core_height_m() * 0.1f;
        q_vol_flat.assign(grid.rows * grid.cols, 0.0f);
        fluxDirty = true;
        transientHistory.clear();
        _lastRecordTime = -1.0f;
    };

    SimPanel     simPanel;
    HeatmapPanel heatmapPanel;
    XenonPanel   xenonPanel;

    float maxDeltaT            = 999.0f;
    float convergenceThreshold = 0.05f;
    bool  converged            = false;
    bool  needReset            = false;

    std::vector<float> T_prev(grid.cubes.size(), params.tempEntree);

    // ── Boucle principale ─────────────────────────────────────
    while (!WindowShouldClose()) {

        camera.update();
        updateRenderOptions(renderOpt);

        // ============================================================
        //  Simulation
        // ============================================================
        if (thermalAvailable && simCtrl.state == SimState::RUNNING) {

            bool doStep = true;
            if (simCtrl.frameSkipMax > 0) {
                simCtrl.frameSkip++;
                doStep = (simCtrl.frameSkip > simCtrl.frameSkipMax);
                if (doStep) simCtrl.frameSkip = 0;
            }

            if (doStep) {
                // Sauvegarde T précédent
                if ((int)T_prev.size() != (int)grid.cubes.size())
                    T_prev.resize(grid.cubes.size());
                for (int i = 0; i < (int)grid.cubes.size(); ++i)
                    T_prev[i] = grid.cubes[i].temperature;

                // ── Neutronique (CPU+GPU) ───────────────────────
                if (neutronAvailable) {
                    // Mettre à jour T_mod moyen depuis le caloporteur
                    if (!grid.cubes.empty()) {
                        float tsum = 0.0f;
                        for (const auto& cu : grid.cubes)
                            tsum += cu.temperature;
                        neutronCompute.T_mod_avg = tsum / (float)grid.cubes.size();
                    }
                    neutronCompute.C_bore_ppm = xenonPanel.C_bore_ppm;
                    neutronCompute.rebuildXS(grid, params.tempEntree,
                                            reactorCfg.moderateur);
                    neutronCompute.step(5, 3);
                    neutronCompute.applyToGrid(grid);

                    // Enregistrement historique Xénon
                    xenonPanel.recordPoint(simCtrl.simTime,
                                           neutronCompute.xenon,
                                           neutronCompute.power_rel);
                }

                recalcFlux(simCtrl.fluxMode);

                // ── Thermique GPU ───────────────────────────────
                thermalCompute.step(simCtrl.stepsPerFrame, q_vol_flat);
                thermalCompute.applyToGrid(grid, meshConfig.sub_xy);

                if (coolantPanel.active)
                    coolantModel.update(grid);

                // ── ΔT max ──────────────────────────────────────
                maxDeltaT = 0.0f;
                for (int i = 0; i < (int)grid.cubes.size(); ++i)
                    maxDeltaT = fmaxf(maxDeltaT,
                        fabsf(grid.cubes[i].temperature - T_prev[i]));

                simCtrl.simTime += thermalCompute.params.dt
                                 * simCtrl.stepsPerFrame;
                simCtrl.T_min = grid.tempMin;
                simCtrl.T_max = grid.tempMax;

                // ── Historique transitoire ──────────────────────
                if (simCtrl.simTime - _lastRecordTime >= recordInterval) {
                    _lastRecordTime = simCtrl.simTime;
                    float T_sum = 0.0f;
                    for (const auto& cu : grid.cubes) T_sum += cu.temperature;
                    float T_avg = grid.cubes.empty() ? params.tempEntree
                                : T_sum / (float)grid.cubes.size();
                    transientHistory.push_back({
                        simCtrl.simTime,
                        neutronAvailable ? neutronCompute.k_eff     : 1.0f,
                        neutronAvailable ? neutronCompute.reactivity : 0.0f,
                        neutronAvailable ? neutronCompute.power_rel  : 1.0f,
                        simCtrl.T_max,
                        T_avg
                    });
                    if ((int)transientHistory.size() > 5000)
                        transientHistory.erase(transientHistory.begin());
                }
            }
        }

        // ── Reset ─────────────────────────────────────────────
        // Déclenché UNIQUEMENT par le bouton Reset du SimPanel
        if (simCtrl.resetRequested) {
            needReset = true;
            simCtrl.resetRequested = false;
        }

        if (needReset && simCtrl.state == SimState::STOPPED
            && thermalAvailable)
        {
            needReset = false;
            thermalCompute.reset(params.tempEntree);
            thermalCompute.applyToGrid(grid, meshConfig.sub_xy);
            for (auto& c : grid.cubes) c.temperature = params.tempEntree;
            simCtrl.simTime   = 0.0f;
            simCtrl.T_min     = params.tempEntree;
            simCtrl.T_max     = params.tempEntree;
            maxDeltaT         = 999.0f;
            converged         = false;
            simCtrl.frameSkip = 0;
            transientHistory.clear();
            _lastRecordTime   = -1.0f;
            xenonPanel.reset();
            coolantModel.init(grid, coolantParams, meshConfig.n_assy_cols, meshConfig.n_assy_rows);

            if (neutronAvailable) {
                grid.autoGenerateZones(reactorCfg.nReflectorRings);
                // NeutronCompute v2 : le destructeur nettoie le GPU,
                // on réinitialise en place via init()
                neutronAvailable = neutronCompute.init(
                    vkCtx, grid, reactorCfg.reactorType,
                    reactorCfg.enrichment, params.tempEntree);
                if (neutronAvailable)
                    simCtrl.fluxMode = FluxMode::DIFFUSION_2G;
            }

            neutronFlux.init(thermalCompute.params.dt,
                             thermalCompute.params.rho_cp, 2.0f);
            fluxDirty = true;
            recalcFlux(simCtrl.fluxMode);
        }

        // ── Changement tranches axiales ───────────────────────
        if (simCtrl.slicesChanged && thermalAvailable) {
            simCtrl.slicesChanged = false;
            // [Mesh] Mettre à jour meshConfig.slices avant reinit
            meshConfig = meshConfig;  // déjà à jour via SimPanel
            // SimPanel modifie simCtrl.nSlices directement
            thermalCompute.reinit(vkCtx, grid,
                                  params.tempEntree, simCtrl.nSlices);
            grid.slices       = simCtrl.nSlices;
            simCtrl.dt_current = thermalCompute.params.dt;
            neutronFlux.init(thermalCompute.params.dt,
                             thermalCompute.params.rho_cp, 2.0f);
            thermalCompute.applyToGrid(grid, meshConfig.sub_xy);
            converged    = false;
            maxDeltaT    = 999.0f;
            simCtrl.simTime = 0.0f;
            fluxDirty    = true;
        }

        // ── Panneau réacteur ─────────────────────────────────
        if (reactorCfg.changed) {
            reactorCfg.changed = false;
            params.enrichissement = reactorCfg.enrichment * 100.0f;
            params.moderateur     = reactorCfg.moderateur;
            params.puissance      = reactorCfg.puissance;

            grid.autoGenerateZones(reactorCfg.nReflectorRings);

            // Réinit neutronique avec nouveaux paramètres
            neutronAvailable = neutronCompute.init(
                vkCtx, grid, reactorCfg.reactorType,
                reactorCfg.enrichment, params.tempEntree);

            converged       = false;
            maxDeltaT       = 999.0f;
            simCtrl.simTime = 0.0f;
            fluxDirty       = true;
        }

        // ============================================================
        //  Rendu
        // ============================================================
        BeginDrawing();
        ClearBackground({25,25,30,255});

        BeginMode3D(camera.get());
            drawGrid3DAxial(grid, renderOpt, meshConfig.sub_xy, meshConfig.sub_z);
            modRenderer.showModerator  = reactorCfg.showModerator;
            modRenderer.showReflector  = reactorCfg.showReflector;
            modRenderer.showControlRod = reactorCfg.showControlRod;
            modRenderer.draw(grid, renderOpt, (float)GetTime());
            coolantPanel.draw3DArrows(grid, coolantModel, renderOpt, meshConfig.n_assy_cols, meshConfig.n_assy_rows);
        EndMode3D();

        // ── Panneaux UI ──────────────────────────────────────
        reactorPanel.update(reactorCfg,
            neutronAvailable ? neutronCompute.k_eff : 1.0f,
            simCtrl, SW, SH);

        drawHUD(SW, SH, params, grid, renderOpt);
        drawConvergenceOverlay(SW, SH, simCtrl, maxDeltaT,
                               convergenceThreshold, converged,
                               neutronAvailable ? neutronCompute.k_eff : 1.0f,
                               neutronAvailable);

        // ── [Mesh2] DimsPanel v2 — touche D ──────────────────
        dimsPanel.update(SW, SH);
        meshPanel.update(SW, SH);
        // (les changements sont gérés via les callbacks onChange)

        // ── SimPanel — touche S ───────────────────────────────
        bool simChanged = simPanel.update(
            simCtrl, SW, SH,
            neutronAvailable,
            neutronAvailable && neutronCompute.gpuAccel,
            transientHistory,
            neutronAvailable ? neutronCompute.reactivity : 0.0f,
            neutronAvailable ? neutronCompute.power_rel  : 1.0f);
        if (simChanged) fluxDirty = true;
        if (fluxDirty && simCtrl.state != SimState::RUNNING)
            recalcFlux(simCtrl.fluxMode);

        // ── Caloporteur ───────────────────────────────────────
        if (IsKeyPressed(KEY_F))
            coolantPanel.displayMode = (CoolantDisplayMode)(
                ((int)coolantPanel.displayMode + 1) % 3);

        if (coolantPanel.updatePanel(coolantParams, simCtrl, SW, SH))
            coolantModel.init(grid, coolantParams, meshConfig.n_assy_cols, meshConfig.n_assy_rows);

        coolantPanel.draw2DOverlay(grid, coolantModel, SW, SH, meshConfig.n_assy_cols, meshConfig.n_assy_rows);
        coolantPanel.drawInfoOverlay(coolantModel, SW);

        heatmapPanel.draw(grid, meshConfig, SW, SH);

        // ── XenonPanel — touche X ─────────────────────────────
        if (neutronAvailable) {
            bool isREP = (reactorCfg.reactorType == ReactorType::REP);
            bool boreChanged = xenonPanel.update(
                neutronCompute.xenon,
                simCtrl.simTime,
                neutronCompute.power_rel,
                isREP, SW, SH);
            if (boreChanged) {
                neutronCompute.C_bore_ppm = xenonPanel.C_bore_ppm;
                fluxDirty = true;
            }
        }

        // ── Bandeau bas état ──────────────────────────────────
        if (simCtrl.state == SimState::RUNNING ||
            simCtrl.state == SimState::PAUSED)
        {
            char buf[160];
            float rho_pcm = neutronAvailable
                          ? neutronCompute.reactivity * 1e5f : 0.0f;
            float xe_pcm  = (neutronAvailable && neutronCompute.xenonActive)
                          ? neutronCompute.xenon.totalPoisoning_pcm() : 0.0f;
            snprintf(buf, sizeof(buf),
                "t=%.0f s  k=%.5f  rho=%+.0f pcm  Xe=%+.0f pcm  "
                "T_max=%.1f C  %dx%dx%d",
                simCtrl.simTime,
                neutronAvailable ? neutronCompute.k_eff : 1.0f,
                rho_pcm, xe_pcm,
                simCtrl.T_max,
                meshConfig.cols, meshConfig.rows, meshConfig.slices);
            int tw = MeasureText(buf, 12);
            Color bc = (fabsf(rho_pcm) < 100.f) ? Color{0,60,0,220}
                     : (rho_pcm > 0)             ? Color{80,30,0,220}
                                                 : Color{0,30,80,220};
            DrawRectangle(SW/2-tw/2-10, SH-38, tw+20, 20, bc);
            DrawText(buf, SW/2-tw/2, SH-35, 12, {200,255,200,255});
        }

        // ── Panneau "Modèle actif" — coin haut-droite ─────────
        {
            const int PW = 255, PX = SW - PW - 6, PY = 6;
            int py = PY + 6;
            DrawRectangle(PX-4, PY, PW, 195, {5,8,20,230});
            DrawRectangleLines(PX-4, PY, PW, 195, {60,80,140,200});

            DrawText("MODELE ACTIF", PX, py, 11, {120,160,255,255}); py += 15;
            DrawLine(PX-4, PY+20, PX-4+PW, PY+20, {40,60,120,180}); py += 3;

            char buf[90];

            // Maillage
            snprintf(buf, sizeof(buf), "Grille : %dx%dx%d  hex struct.",
                     meshConfig.cols, meshConfig.rows, meshConfig.slices);
            DrawText(buf, PX, py, 10, {160,180,220,200}); py += 12;
            snprintf(buf, sizeof(buf), "sub_xy=%d  sub_z=%d  AR=%.2f  %s",
                     meshConfig.sub_xy, meshConfig.sub_z,
                     meshConfig.aspect_ratio,
                     meshConfig.aspectOK() ? "OK" : "DEGRADE!");
            Color ar_col = meshConfig.aspectOK()
                ? Color{120,200,120,200} : Color{255,140,40,255};
            DrawText(buf, PX, py, 10, ar_col); py += 15;

            // ── Tableau GPU/CPU par phénomène ─────────────────
            DrawText("Phenomene", PX,     py, 10, {120,140,180,220});
            DrawText("Calcul",    PX+152, py, 10, {120,140,180,220});
            py += 12;
            DrawLine(PX-4, py, PX-4+PW, py, {30,50,100,150}); py += 4;

            auto row = [&](const char* label, bool onGPU, bool active){
                Color lc = active ? Color{200,210,230,255} : Color{80,90,100,180};
                DrawText(label, PX, py, 10, lc);
                if (active) {
                    Color gc = onGPU ? Color{80,255,120,255} : Color{255,200,80,255};
                    DrawRectangle(PX+148, py-1, onGPU?38:30, 12,
                        onGPU ? Color{20,60,20,180} : Color{60,50,10,180});
                    DrawText(onGPU ? "GPU" : "CPU", PX+152, py, 10, gc);
                } else {
                    DrawText("--", PX+152, py, 10, {60,70,80,180});
                }
                py += 13;
            };

            bool gpuNeutron = neutronAvailable && neutronCompute.gpuAccel;
            bool simRunning = (simCtrl.state == SimState::RUNNING);

            row("Diffusion thermique 3D",  true,        thermalAvailable);
            row("Diffusion neutronique 2G", gpuNeutron, neutronAvailable);
            row("Sections efficaces XS",   false,       neutronAvailable);
            row("Precurseurs 6G (Keepin)", false,       neutronAvailable);
            row("Xenon-135 / Iode-135",    false,       neutronAvailable && neutronCompute.xenonActive);
            row("Caloporteur 1D",          false,       coolantPanel.active);

            py += 2;
            DrawLine(PX-4, py, PX-4+PW, py, {30,50,100,150}); py += 5;
            DrawText("[D] Resolution  [P] Dimensions  [X] Xenon", PX, py, 10, {80,110,180,200});
        }

        // ── Barre de commandes ────────────────────────────────
        DrawText(
            "[S]Sim [D]Resolution [P]Dimensions [X]Xenon [C]Caloporteur [F]Fluide "
            "[H]Heatmap [T]Colormap [G]Grille [W]Fils [R]Reacteur",
            10, SH-20, 11, {80,80,80,255});

        // ── Guide démarrage ───────────────────────────────────
        if (simCtrl.simTime < 0.1f && GetTime() < 14.0
            && simCtrl.state == SimState::STOPPED)
        {
            int gx = SW/2 - 250, gy = SH/2 - 100;
            DrawRectangle(gx, gy, 500, 178, {5,15,30,230});
            DrawRectangleLines(gx, gy, 500, 178, {80,200,80,255});
            DrawText("SIMULATION TRANSITOIRE v2", gx+10, gy+8, 14,
                     {100,255,100,255});
            DrawText("1. [R]  Choisir technologie + enrichissement",
                     gx+10, gy+30, 12, LIGHTGRAY);
            DrawText("        Cliquer  Appliquer",
                     gx+10, gy+46, 12, {100,220,100,255});
            DrawText("2. [D]  Resolution  [P]  Dimensions physiques",
                     gx+10, gy+62, 12, {100,200,255,255});
            DrawText("        Colonnes x rangees x tranches calcules automatiquement",
                     gx+10, gy+78, 11, {80,160,200,255});
            DrawText("3. [C]  Choisir caloporteur + activer",
                     gx+10, gy+94, 12, LIGHTGRAY);
            DrawText("4. [S]  Start — simulation continue",
                     gx+10, gy+110, 12, LIGHTGRAY);
            DrawText("        Courbes k_eff / P(t) / T(t) dans panneau [S]",
                     gx+10, gy+126, 12, {100,200,255,255});
            DrawText("5. [X]  Xénon-135 : empoisonnement + historique",
                     gx+10, gy+142, 12, {200,150,255,255});
            char tb[32];
            snprintf(tb, sizeof(tb), "(disparait dans %.0fs)",
                     14.0 - GetTime());
            DrawText(tb, gx+360, gy+164, 10, {80,80,80,255});
        }

        EndDrawing();
    }

    // ── Nettoyage ─────────────────────────────────────────────
    // NeutronCompute v2 : nettoyage GPU via destructeur (~NeutronCompute)
    thermalCompute.cleanup();
    vkCtx.cleanup();
    CloseWindow();
    return 0;
}