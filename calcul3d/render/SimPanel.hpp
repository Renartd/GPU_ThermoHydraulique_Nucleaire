#pragma once
// ============================================================
//  SimPanel.hpp
//  Panneau de contrôle de la simulation thermique
//  Touche S : ouvrir/fermer
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <string>
#include "../physics/NeutronFlux.hpp"

enum class SimMode { ACCELERE, TEMPS_REEL };
enum class SimState { STOPPED, RUNNING, PAUSED };

struct SimControl {
    SimMode  mode    = SimMode::ACCELERE;
    SimState state   = SimState::STOPPED;
    FluxMode fluxMode = FluxMode::COSINUS_REP;

    float speedFactor = 10.0f;   // accélération (1 frame = Nx dt)
    float simTime     = 0.0f;    // temps simulé total (s)
    float T_min       = 286.0f;
    float T_max       = 286.0f;

    int   stepsPerFrame = 10;    // nombre de pas GPU par frame
};

struct SimPanel {
    bool visible = false;

    bool update(SimControl& ctrl, int sw, int sh) {
        if (IsKeyPressed(KEY_S)) visible = !visible;
        if (!visible) return false;

        const int PX = sw/2 - 180, PY = sh/2 - 170;
        const int PW = 360,        PH = 340;

        // Fond
        DrawRectangle(PX, PY, PW, PH, {10,10,25,240});
        DrawRectangleLines(PX, PY, PW, PH, {100,150,220,255});
        DrawText("SIMULATION THERMIQUE", PX+10, PY+10, 14, LIGHTGRAY);

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed  = false;

        int y = PY + 35;

        // --- Flux neutronique ---
        DrawText("Flux neutronique :", PX+10, y, 12, {180,180,180,255});
        y += 18;

        auto radioBtn = [&](const char* label, bool active, int rx, int ry) -> bool {
            Color col = active ? Color{100,200,100,255} : Color{80,80,80,255};
            DrawCircle(rx+6, ry+6, 6, col);
            DrawCircleLines(rx+6, ry+6, 6, LIGHTGRAY);
            DrawText(label, rx+16, ry, 12, LIGHTGRAY);
            Rectangle r = {(float)rx, (float)ry, 140, 14};
            return clicked && CheckCollisionPointRec(mouse, r);
        };

        if (radioBtn("Cosinus REP", ctrl.fluxMode == FluxMode::COSINUS_REP,
                     PX+15, y))
            { ctrl.fluxMode = FluxMode::COSINUS_REP; changed = true; }
        if (radioBtn("Uniforme",    ctrl.fluxMode == FluxMode::UNIFORME,
                     PX+170, y))
            { ctrl.fluxMode = FluxMode::UNIFORME; changed = true; }
        y += 28;

        // --- Mode temps ---
        DrawText("Mode temporel :", PX+10, y, 12, {180,180,180,255});
        y += 18;
        if (radioBtn("Accelere", ctrl.mode == SimMode::ACCELERE, PX+15, y))
            ctrl.mode = SimMode::ACCELERE;
        if (radioBtn("Temps reel", ctrl.mode == SimMode::TEMPS_REEL, PX+170, y))
            ctrl.mode = SimMode::TEMPS_REEL;
        y += 28;

        // --- Vitesse ---
        DrawText("Vitesse simulation :", PX+10, y, 12, {180,180,180,255});
        y += 18;
        char buf[64];
        snprintf(buf, sizeof(buf), "x%.0f  [+/-]", ctrl.speedFactor);
        DrawText(buf, PX+15, y, 13, {255,220,80,255});

        // Barre de vitesse
        float barW = PW - 30.0f;
        float ratio = (log10f(ctrl.speedFactor) - 0.0f) / 3.0f; // 1x → 1000x
        DrawRectangle(PX+15, y+16, (int)barW, 8, {40,40,60,255});
        DrawRectangle(PX+15, y+16, (int)(barW * ratio), 8, {80,160,255,255});
        DrawRectangleLines(PX+15, y+16, (int)barW, 8, {80,80,120,255});

        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD))
            ctrl.speedFactor = fminf(ctrl.speedFactor * 1.05f, 1000.0f);
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT))
            ctrl.speedFactor = fmaxf(ctrl.speedFactor / 1.05f, 1.0f);

        ctrl.stepsPerFrame = (int)fmaxf(1.0f, ctrl.speedFactor);
        y += 34;

        // --- Stats ---
        DrawRectangle(PX+10, y, PW-20, 58, {20,20,40,200});
        DrawRectangleLines(PX+10, y, PW-20, 58, {60,60,90,255});
        snprintf(buf, sizeof(buf), "Temps simule : %.1f s", ctrl.simTime);
        DrawText(buf, PX+18, y+6,  12, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "T min : %.1f C   T max : %.1f C",
                 ctrl.T_min, ctrl.T_max);
        DrawText(buf, PX+18, y+22, 12, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "Pas GPU/frame : %d", ctrl.stepsPerFrame);
        DrawText(buf, PX+18, y+38, 12, {120,120,180,255});
        y += 68;

        // --- Boutons Start/Pause/Reset ---
        struct Btn { const char* label; Color col; Color hov; SimState tgt; } btns[3] = {
            {"  Start",  {35,90,35,255},  {50,130,50,255},  SimState::RUNNING},
            {"  Pause",  {80,80,20,255},  {110,110,30,255}, SimState::PAUSED},
            {"  Reset",  {90,35,35,255},  {130,50,50,255},  SimState::STOPPED},
        };
        for (int i = 0; i < 3; ++i) {
            Rectangle btn = {(float)(PX+10 + i*110), (float)y, 100, 28};
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

        // --- État courant ---
        const char* stateStr =
            ctrl.state == SimState::RUNNING ? "EN COURS" :
            ctrl.state == SimState::PAUSED  ? "EN PAUSE" : "ARRETE";
        Color stateCol =
            ctrl.state == SimState::RUNNING ? Color{100,255,100,255} :
            ctrl.state == SimState::PAUSED  ? Color{255,200,50,255}  :
                                              Color{180,80,80,255};
        DrawText(stateStr, PX+PW/2-30, y, 13, stateCol);

        // Fermer sur clic hors panneau
        Rectangle panelRect = {(float)PX,(float)PY,(float)PW,(float)PH};
        if (clicked && !CheckCollisionPointRec(mouse, panelRect))
            visible = false;

        return changed;
    }
};
