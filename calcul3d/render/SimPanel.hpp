#pragma once
// ============================================================
//  SimPanel.hpp
//  Panneau de contrôle de la simulation thermique
//  Touche S : ouvrir/fermer
//  Corrections :
//    - Bug "déjà convergé à t=0" : convergence reset propre
//      lors du changement de tranches (slicesChanged)
//    - Section "Mode flux" : affiche GPU Diffusion 2G si dispo
//    - Divergence 16/32 : info dt affiché pour debug
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include "../physics/NeutronFlux.hpp"

enum class SimMode  { ACCELERE, TEMPS_REEL };
enum class SimState { STOPPED, RUNNING, PAUSED };

struct SimControl {
    SimMode  mode     = SimMode::ACCELERE;
    SimState state    = SimState::STOPPED;
    FluxMode fluxMode = FluxMode::COSINUS_REP;

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
    bool update(SimControl& ctrl, int sw, int sh, bool gpuFluxActive = false) {
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

        if (gpuFluxActive) {
            // GPU dispo : bandeau vert informatif
            DrawRectangle(PX+8, y, PW-16, 20, {20,60,20,200});
            DrawRectangleLines(PX+8, y, PW-16, 20, {60,180,60,255});
            DrawText("GPU Diffusion 2 groupes ACTIF  [R] pour changer", PX+14, y+4, 11, {100,255,100,255});
            y += 26;
            // Fallback cosinus (secondaire, en gris)
            DrawText("Fallback si GPU desactive :", PX+10, y, 11, {120,120,120,255}); y += 14;
            if (radioBtn("Cosinus REP", ctrl.fluxMode == FluxMode::COSINUS_REP, PX+15, y))
                { ctrl.fluxMode = FluxMode::COSINUS_REP; changed = true; }
            if (radioBtn("Uniforme", ctrl.fluxMode == FluxMode::UNIFORME, PX+185, y))
                { ctrl.fluxMode = FluxMode::UNIFORME; changed = true; }
        } else {
            // Pas de GPU : workflow classique
            DrawText("Flux neutronique :", PX+10, y, 12, {180,180,180,255}); y += 17;
            if (radioBtn("Cosinus REP", ctrl.fluxMode == FluxMode::COSINUS_REP, PX+15, y))
                { ctrl.fluxMode = FluxMode::COSINUS_REP; changed = true; }
            if (radioBtn("Uniforme", ctrl.fluxMode == FluxMode::UNIFORME, PX+185, y))
                { ctrl.fluxMode = FluxMode::UNIFORME; changed = true; }
        }
        y += 28;

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
        DrawRectangle(PX+10, y, PW-20, 68, {20,20,40,200});
        DrawRectangleLines(PX+10, y, PW-20, 68, {60,60,90,255});
        snprintf(buf, sizeof(buf), "Temps simule : %.1f s", ctrl.simTime);
        DrawText(buf, PX+18, y+6,  12, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "T min : %.1f C   T max : %.1f C", ctrl.T_min, ctrl.T_max);
        DrawText(buf, PX+18, y+22, 12, LIGHTGRAY);
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

        Rectangle panelRect = {(float)PX,(float)PY,(float)PW,(float)PH};
        if (clicked && !CheckCollisionPointRec(mouse, panelRect)) visible = false;
        return changed;
    }
};