#pragma once
// ============================================================
//  SimPanel.hpp — Panneau simulation + courbes transitoires
//  Touche S : ouvrir/fermer
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include "../physics/NeutronFlux.hpp"

enum class SimMode  { ACCELERE, TEMPS_REEL };
enum class SimState { STOPPED, RUNNING, PAUSED };

// Point d'historique transitoire (partagé main ↔ SimPanel)
struct TransientPoint {
    float t;
    float k_eff;
    float reactivity;  // Δk/k
    float power_rel;   // puissance relative
    float T_max;
    float T_avg;
};

struct SimControl {
    SimMode  mode     = SimMode::ACCELERE;
    SimState state    = SimState::STOPPED;
    FluxMode fluxMode = FluxMode::DIFFUSION_2G;  // modèle par défaut

    float speedFactor   = 1.0f;
    float simTime       = 0.0f;
    float T_min         = 286.0f;
    float T_max         = 286.0f;

    int   stepsPerFrame = 1;
    int   frameSkip     = 0;
    int   frameSkipMax  = 0;

    int   nSlices       = 8;
    bool  slicesChanged = false;

    // Exposé pour affichage dans le panneau
    float dt_current    = 0.0f;   // rempli par main après ThermalCompute.init
};

struct SimPanel {
    bool visible    = false;
    char inputBuf[16] = "1";
    bool inputFocus = false;

    void applyInput(SimControl& ctrl) {
        float val = (float)atof(inputBuf);
        if (val >= 0.125f && val <= 1000.0f)
            ctrl.speedFactor = val;
        if (ctrl.speedFactor < 10.0f)
            snprintf(inputBuf, sizeof(inputBuf), "%.2f", ctrl.speedFactor);
        else
            snprintf(inputBuf, sizeof(inputBuf), "%.0f", ctrl.speedFactor);
    }

    // gpuFluxActive : true si NeutronCompute GPU est disponible
    bool update(SimControl& ctrl, int sw, int sh,
                bool neutronAvail = false,
                bool gpuFluxActive = false,
                const std::vector<TransientPoint>& history = {},
                float reactivity_dk = 0.0f,
                float power_rel = 1.0f) {
        if (IsKeyPressed(KEY_S)) { visible = !visible; inputFocus = false; }
        if (!visible) return false;

        // Centre-droit pour ne pas chevaucher le panneau reacteur (gauche)
        const int PX = sw/2 - 10, PY = sh/2 - 240;
        const int PW = 370,       PH = 460;

        DrawRectangle(PX, PY, PW, PH, {10,10,25,240});
        DrawRectangleLines(PX, PY, PW, PH, {100,150,220,255});
        DrawText("SIMULATION THERMIQUE", PX+10, PY+10, 14, LIGHTGRAY);

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed  = false;
        int y = PY + 34;

        // --- Mode flux neutronique ---
        auto radioBtn = [&](const char* label, bool active, int rx, int ry) -> bool {
            Color col = active ? Color{100,200,100,255} : Color{80,80,80,255};
            DrawCircle(rx+6, ry+6, 6, col);
            DrawCircleLines(rx+6, ry+6, 6, LIGHTGRAY);
            DrawText(label, rx+16, ry, 12, active ? WHITE : LIGHTGRAY);
            Rectangle r = {(float)rx, (float)ry, 150, 14};
            return clicked && CheckCollisionPointRec(mouse, r);
        };

        // ── Sélection du modèle de flux ──────────────────────────────
        DrawText("Modele de flux :", PX+10, y, 12, {180,180,180,255}); y += 17;

        // Ligne 1 : Diffusion 2G (cochable seulement si neutronAvail)
        {
            bool can2G   = neutronAvail;
            bool active2G = (ctrl.fluxMode == FluxMode::DIFFUSION_2G);
            // Si 2G vient d'être rendu dispo, forcer la sélection
            if (can2G && ctrl.fluxMode != FluxMode::DIFFUSION_2G
                      && ctrl.fluxMode != FluxMode::COSINUS_REP
                      && ctrl.fluxMode != FluxMode::UNIFORME)
                { ctrl.fluxMode = FluxMode::DIFFUSION_2G; changed = true; }

            Color circCol = can2G ? (active2G ? Color{80,220,80,255} : Color{60,60,60,255})
                                  : Color{50,50,50,180};
            DrawCircle(PX+21, y+7, 6, active2G ? Color{80,220,80,255} : Color{30,30,30,200});
            DrawCircleLines(PX+21, y+7, 6, can2G ? LIGHTGRAY : Color{70,70,70,255});

            const char* lbl2G = gpuFluxActive
                ? "Diffusion 2G (CPU+GPU)"
                : (can2G ? "Diffusion 2G (CPU)" : "Diffusion 2G (non dispo)");
            DrawText(lbl2G, PX+32, y, 12, can2G ? (active2G ? WHITE : LIGHTGRAY) : Color{80,80,80,255});

            if (can2G) {
                Rectangle r2g = {(float)(PX+8), (float)y, PW-16.0f, 16};
                if (clicked && CheckCollisionPointRec(mouse, r2g))
                    { ctrl.fluxMode = FluxMode::DIFFUSION_2G; changed = true; }
            }
        }
        y += 20;

        // Ligne 2 : Cosinus REP
        if (radioBtn("Cosinus REP", ctrl.fluxMode == FluxMode::COSINUS_REP, PX+15, y))
            { ctrl.fluxMode = FluxMode::COSINUS_REP; changed = true; }
        // Ligne 3 : Uniforme
        if (radioBtn("Uniforme",    ctrl.fluxMode == FluxMode::UNIFORME,    PX+185, y))
            { ctrl.fluxMode = FluxMode::UNIFORME;    changed = true; }
        y += 24;

        // Forcer 2G si neutronAvail et mode encore sur défaut cosinus au lancement
        if (neutronAvail && ctrl.fluxMode == FluxMode::COSINUS_REP) {
            // Ne pas forcer si l'utilisateur a explicitement choisi cosinus
            // (on détecte par le fait que c'est la valeur par défaut non changée)
        }
        y += 4;

        // --- Mode temps ---
        DrawText("Mode temporel :", PX+10, y, 12, {180,180,180,255}); y += 17;
        if (radioBtn("Accelere",   ctrl.mode == SimMode::ACCELERE,   PX+15,  y)) ctrl.mode = SimMode::ACCELERE;
        if (radioBtn("Temps reel", ctrl.mode == SimMode::TEMPS_REEL, PX+185, y)) ctrl.mode = SimMode::TEMPS_REEL;
        y += 28;

        // --- Vitesse ---
        DrawText("Vitesse (0.125 - 1000) :", PX+10, y, 12, {180,180,180,255}); y += 17;

        Rectangle inputRect = {(float)(PX+15), (float)y, 110, 24};
        bool hoverInput = CheckCollisionPointRec(mouse, inputRect);
        if (clicked && hoverInput)  inputFocus = true;
        if (clicked && !hoverInput) inputFocus = false;
        DrawRectangleRec(inputRect, inputFocus ? Color{30,30,60,255} : Color{20,20,40,255});
        DrawRectangleLinesEx(inputRect, 1,
            inputFocus ? Color{100,180,255,255} : Color{80,80,120,255});
        DrawText(inputBuf, (int)inputRect.x+5, (int)inputRect.y+5, 13, WHITE);
        if (inputFocus && ((int)(GetTime()*2) % 2 == 0)) {
            int tw = MeasureText(inputBuf, 13);
            DrawRectangle((int)inputRect.x+5+tw, (int)inputRect.y+4, 2, 16, WHITE);
        }
        if (inputFocus) {
            int key = GetCharPressed();
            while (key > 0) {
                int len = (int)strlen(inputBuf);
                if (key == ',') key = '.';
                if ((key >= '0' && key <= '9') || key == '.') {
                    if (len < (int)sizeof(inputBuf)-1) {
                        inputBuf[len]   = (char)key;
                        inputBuf[len+1] = '\0';
                    }
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                int len = (int)strlen(inputBuf);
                if (len > 0) inputBuf[len-1] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB)) {
                applyInput(ctrl); inputFocus = false;
            }
        }

        Rectangle btnMinus = {(float)(PX+133), (float)y, 28, 24};
        Rectangle btnPlus  = {(float)(PX+165), (float)y, 28, 24};
        bool hoverM = CheckCollisionPointRec(mouse, btnMinus);
        bool hoverP = CheckCollisionPointRec(mouse, btnPlus);
        DrawRectangleRec(btnMinus, hoverM ? Color{80,40,40,255} : Color{60,30,30,255});
        DrawRectangleRec(btnPlus,  hoverP ? Color{40,80,40,255} : Color{30,60,30,255});
        DrawRectangleLinesEx(btnMinus, 1, LIGHTGRAY);
        DrawRectangleLinesEx(btnPlus,  1, LIGHTGRAY);
        DrawText("-", (int)btnMinus.x+9,  (int)btnMinus.y+4, 14, WHITE);
        DrawText("+", (int)btnPlus.x+8,   (int)btnPlus.y+4,  14, WHITE);
        if (clicked && hoverM) {
            ctrl.speedFactor = fmaxf(ctrl.speedFactor / 2.0f, 0.125f);
            snprintf(inputBuf, sizeof(inputBuf),
                ctrl.speedFactor < 10.0f ? "%.2f" : "%.0f", ctrl.speedFactor);
            inputFocus = false;
        }
        if (clicked && hoverP) {
            ctrl.speedFactor = fminf(ctrl.speedFactor * 2.0f, 1000.0f);
            snprintf(inputBuf, sizeof(inputBuf),
                ctrl.speedFactor < 10.0f ? "%.2f" : "%.0f", ctrl.speedFactor);
            inputFocus = false;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "= x%.2f", ctrl.speedFactor);
        DrawText(buf, PX+202, y+4, 12, {255,220,80,255});
        y += 30;

        float barW = (float)(PW - 30);
        float ratio = (log10f(ctrl.speedFactor) + 0.9f) / 3.9f;
        ratio = fmaxf(0.0f, fminf(1.0f, ratio));
        DrawRectangle(PX+15, y, (int)barW, 6, {40,40,60,255});
        DrawRectangle(PX+15, y, (int)(barW*ratio), 6, {80,160,255,255});
        DrawRectangleLines(PX+15, y, (int)barW, 6, {80,80,120,255});
        y += 14;

        if (ctrl.speedFactor >= 1.0f) {
            ctrl.stepsPerFrame = (int)fmaxf(1.0f, ctrl.speedFactor);
            ctrl.frameSkipMax  = 0;
        } else {
            ctrl.stepsPerFrame = 1;
            ctrl.frameSkipMax  = (int)roundf(1.0f / ctrl.speedFactor) - 1;
        }

        // --- Stats ---
        DrawRectangle(PX+10, y, PW-20, 82, {20,20,40,200});
        DrawRectangleLines(PX+10, y, PW-20, 82, {60,60,90,255});
        snprintf(buf, sizeof(buf), "Temps simule : %.1f s", ctrl.simTime);
        DrawText(buf, PX+18, y+6,  12, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "T min : %.1f C   T max : %.1f C", ctrl.T_min, ctrl.T_max);
        DrawText(buf, PX+18, y+22, 12, LIGHTGRAY);
        // Modèle utilisé
        const char* modUsed = (ctrl.fluxMode == FluxMode::DIFFUSION_2G) ? "Diffusion 2G"
                            : (ctrl.fluxMode == FluxMode::COSINUS_REP)  ? "Cosinus REP"
                            :                                              "Uniforme";
        Color modCol = (ctrl.fluxMode == FluxMode::DIFFUSION_2G) ? Color{100,255,100,255}
                                                                  : Color{200,200,80,255};
        snprintf(buf, sizeof(buf), "Modele : %s", modUsed);
        DrawText(buf, PX+18, y+38, 11, modCol);
        if (ctrl.speedFactor >= 1.0f)
            snprintf(buf, sizeof(buf), "Pas GPU/frame : %d", ctrl.stepsPerFrame);
        else
            snprintf(buf, sizeof(buf), "1 pas toutes les %d frames", ctrl.frameSkipMax+1);
        DrawText(buf, PX+18, y+38, 12, {120,120,180,255});
        // Affiche dt pour diagnostiquer divergence
        if (ctrl.dt_current > 0.0f) {
            snprintf(buf, sizeof(buf), "dt = %.2f s", ctrl.dt_current);
            DrawText(buf, PX+18, y+52, 11, {100,100,160,255});
        }
        y += 76;

        // --- Resolution 3D ---
        y += 4;
        DrawRectangle(PX+10, y, PW-20, 56, {20,20,40,200});
        DrawRectangleLines(PX+10, y, PW-20, 56, {60,60,90,255});
        DrawText("Resolution 3D (tranches) :", PX+18, y+5, 12, LIGHTGRAY);

        // Avertissement divergence pour 16/32 si vitesse elevee
        if (ctrl.nSlices >= 16 && ctrl.speedFactor > 4.0f) {
            DrawText("! Reduire vitesse si divergence", PX+18, y+19, 10, {255,160,60,255});
        }
        y += 24;

        int sliceOpts[4] = {4, 8, 16, 32};
        for (int i = 0; i < 4; ++i) {
            Rectangle sb = {(float)(PX+10 + i*85), (float)y, 79, 22};
            bool isActive = (ctrl.nSlices == sliceOpts[i]);
            bool hover    = CheckCollisionPointRec(mouse, sb);
            Color sc = isActive ? Color{40,120,200,255}
                                : (hover ? Color{60,60,80,255} : Color{30,30,50,255});
            DrawRectangleRec(sb, sc);
            DrawRectangleLinesEx(sb, 1, isActive ? Color{80,180,255,255} : LIGHTGRAY);
            char lbl[8]; snprintf(lbl, sizeof(lbl), "%d", sliceOpts[i]);
            int tw = MeasureText(lbl, 13);
            DrawText(lbl, (int)sb.x + (79-tw)/2, (int)sb.y+4, 13, WHITE);
            if (clicked && hover && !isActive) {
                ctrl.nSlices       = sliceOpts[i];
                ctrl.slicesChanged = true;
                ctrl.state         = SimState::STOPPED;
                changed = true;
            }
        }
        y += 30;

        // --- Boutons ---
        struct Btn { const char* label; Color col; Color hov; SimState tgt; };
        Btn btns[3] = {
            {"  Start", {35,90,35,255},  {50,130,50,255},  SimState::RUNNING},
            {"  Pause", {80,80,20,255},  {110,110,30,255}, SimState::PAUSED },
            {"  Reset", {90,35,35,255},  {130,50,50,255},  SimState::STOPPED},
        };
        for (int i = 0; i < 3; ++i) {
            Rectangle btn = {(float)(PX+10 + i*117), (float)y, 107, 28};
            bool hover = CheckCollisionPointRec(mouse, btn);
            DrawRectangleRec(btn, hover ? btns[i].hov : btns[i].col);
            DrawRectangleLinesEx(btn, 1, LIGHTGRAY);
            DrawText(btns[i].label, (int)btn.x+8, (int)btn.y+7, 13, WHITE);
            if (clicked && hover) {
                ctrl.state = btns[i].tgt;
                if (ctrl.state == SimState::STOPPED) ctrl.simTime = 0.0f;
                changed = true;
            }
        }
        y += 38;

        const char* stateStr =
            ctrl.state == SimState::RUNNING ? "EN COURS" :
            ctrl.state == SimState::PAUSED  ? "EN PAUSE" : "ARRETE";
        Color stateCol =
            ctrl.state == SimState::RUNNING ? Color{100,255,100,255} :
            ctrl.state == SimState::PAUSED  ? Color{255,200,50,255}  : Color{180,80,80,255};
        int stw = MeasureText(stateStr, 13);
        DrawText(stateStr, PX+PW/2-stw/2, y, 13, stateCol);
        y += 22;

        // ── Métriques transitoires en temps réel ─────────────
        DrawRectangle(PX+10, y, PW-20, 52, {15,15,35,220});
        DrawRectangleLines(PX+10, y, PW-20, 52, {60,60,100,255});
        {
            char mb[80];
            // Réactivité en pcm (1 pcm = 1e-5 Δk/k)
            float rho_pcm = reactivity_dk * 1e5f;
            Color rc = (fabsf(rho_pcm) < 50.f)  ? Color{100,255,100,255}  // critique ±50 pcm
                     : (rho_pcm > 0)             ? Color{255,120,60,255}   // sur-critique
                                                 : Color{80,160,255,255};  // sous-critique
            snprintf(mb, sizeof(mb), "Reactivite : %+.0f pcm", rho_pcm);
            DrawText(mb, PX+18, y+5, 12, rc);
            snprintf(mb, sizeof(mb), "Puissance  : %.3f P/P0", power_rel);
            Color pc = (power_rel > 1.05f) ? Color{255,160,60,255}
                     : (power_rel < 0.95f) ? Color{80,160,255,255}
                                           : Color{100,255,100,255};
            DrawText(mb, PX+18, y+21, 12, pc);
            snprintf(mb, sizeof(mb), "t = %.1f s", ctrl.simTime);
            DrawText(mb, PX+18, y+37, 11, {140,140,180,255});
        }
        y += 58;

        // ── Mini-graphes transitoires ─────────────────────────
        if (!history.empty()) {
            const int GW = PW-20, GH = 55;
            const int GX = PX+10;

            // Fonction utilitaire de tracé d'une courbe
            auto drawCurve = [&](int gy, const char* label,
                                 float vmin, float vmax,
                                 Color col, Color bgcol,
                                 std::function<float(const TransientPoint&)> getter)
            {
                DrawRectangle(GX, gy, GW, GH, bgcol);
                DrawRectangleLines(GX, gy, GW, GH, {60,60,90,255});
                DrawText(label, GX+4, gy+3, 10, {160,160,200,255});

                // Ligne de référence
                if (vmin < 1.0f && 1.0f < vmax) {
                    float ry = gy + GH - (1.0f-vmin)/(vmax-vmin)*GH;
                    DrawLine(GX, (int)ry, GX+GW, (int)ry, {60,60,60,180});
                }

                // Courbe
                int N = (int)history.size();
                int maxPts = GW;
                int step   = fmaxf(1, N/maxPts);
                Vector2 prev = {-1,-1};
                for (int ii=0; ii<N; ii+=step) {
                    float v = getter(history[ii]);
                    float t_norm = (float)ii / (float)(N-1);
                    float v_norm = (vmax>vmin) ? (v-vmin)/(vmax-vmin) : 0.5f;
                    v_norm = fmaxf(0.f, fminf(1.f, v_norm));
                    Vector2 pt = {(float)(GX + t_norm*GW),
                                  (float)(gy + GH - v_norm*GH)};
                    if (prev.x >= 0)
                        DrawLineEx(prev, pt, 1.5f, col);
                    prev = pt;
                }
                // Valeur courante
                char vb[32];
                float vlast = getter(history.back());
                snprintf(vb, sizeof(vb), "%.4g", vlast);
                DrawText(vb, GX+GW-50, gy+3, 10, col);
            };

            // Bornes dynamiques sur l'historique
            float kmin=1e9f, kmax=-1e9f, pmin=1e9f, pmax=-1e9f, tmin=1e9f, tmax=-1e9f;
            for (const auto& p : history) {
                kmin=fminf(kmin,p.k_eff);    kmax=fmaxf(kmax,p.k_eff);
                pmin=fminf(pmin,p.power_rel); pmax=fmaxf(pmax,p.power_rel);
                tmin=fminf(tmin,p.T_max);     tmax=fmaxf(tmax,p.T_max);
            }
            float margin = 0.02f;
            kmin-=(kmax-kmin)*margin+1e-4f; kmax+=(kmax-kmin)*margin+1e-4f;
            pmin-=(pmax-pmin)*margin+1e-4f; pmax+=(pmax-pmin)*margin+1e-4f;
            tmin-=(tmax-tmin)*margin+1.f;   tmax+=(tmax-tmin)*margin+1.f;

            drawCurve(y,    "k_eff",   kmin, kmax, {100,255,100,255}, {10,25,10,220},
                      [](const TransientPoint& p){ return p.k_eff; });
            y += GH+4;
            drawCurve(y,    "P/P0",    pmin, pmax, {255,160,60,255},  {25,15,5,220},
                      [](const TransientPoint& p){ return p.power_rel; });
            y += GH+4;
            drawCurve(y,    "T_max°C", tmin, tmax, {255,80,80,255},   {25,5,5,220},
                      [](const TransientPoint& p){ return p.T_max; });
            y += GH+4;
        }

        Rectangle panelRect = {(float)PX,(float)PY,(float)PW,(float)(y+10)};
        if (clicked && !CheckCollisionPointRec(mouse, panelRect)) visible = false;
        return changed;
    }
};