#pragma once
// ============================================================
//  CoolantPanel.hpp — Panneau configuration fluide caloporteur
//  Touche C : ouvrir/fermer
//  Touche F : cycle mode affichage (flèches / overlay / flèches+couleur)
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "../physics/CoolantModel.hpp"

enum class CoolantDisplayMode { FLECHES, OVERLAY, FLECHES_COULEUR };

struct CoolantPanel {
    bool    visible      = false;
    bool    active       = true;   // simulation caloporteur activée ?
    CoolantDisplayMode displayMode = CoolantDisplayMode::FLECHES_COULEUR;

    // Saisie T_inlet
    char    bufT[16]  = "286";
    bool    focusT    = false;
    // Saisie P_bar
    char    bufP[16]  = "155";
    bool    focusP    = false;

    float   arrowAnim = 0.0f;   // phase animation flèches (0→1)

    // ---- Toggle affichage avec touche F ----
    void handleKeys() {
        if (IsKeyPressed(KEY_C)) visible = !visible;
        if (IsKeyPressed(KEY_F)) {
            int m = ((int)displayMode + 1) % 3;
            displayMode = (CoolantDisplayMode)m;
        }
    }

    // ---- Panneau configuration ----
    // Retourne true si les paramètres ont changé (reinit nécessaire)
    bool updatePanel(CoolantParams& params, int sw, int sh) {
        handleKeys();
        if (!visible) return false;

        const int PX = sw/2 - 200, PY = sh/2 - 220;
        const int PW = 400,        PH = 430;

        DrawRectangle(PX, PY, PW, PH, {10,15,25,245});
        DrawRectangleLines(PX, PY, PW, PH, {80,180,220,255});
        DrawText("FLUIDE CALOPORTEUR", PX+10, PY+10, 14, {80,200,255,255});

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed  = false;
        int  y        = PY + 32;

        // ---- Type de fluide ----
        DrawText("Type de fluide :", PX+10, y, 12, {180,180,180,255});
        y += 18;

        struct FluidOpt { FluidType ft; const char* label; const char* reactor; };
        FluidOpt opts[4] = {
            { FluidType::EAU,      "Eau legere H2O",  "REP"   },
            { FluidType::SODIUM,   "Sodium Na",        "RNR-Na"},
            { FluidType::PLOMB_BI, "Plomb-Bismuth LBE","RNR-Pb"},
            { FluidType::HELIUM,   "Helium He",        "RHT"   },
        };
        for (int i = 0; i < 4; ++i) {
            bool active = (params.fluid == opts[i].ft);
            Color cc = active ? Color{80,200,100,255} : Color{60,60,60,255};
            DrawCircle(PX+20, y+6, 6, cc);
            DrawCircleLines(PX+20, y+6, 6, LIGHTGRAY);
            DrawText(opts[i].label, PX+32, y, 12, LIGHTGRAY);
            char rbuf[32];
            snprintf(rbuf, sizeof(rbuf), "(%s)", opts[i].reactor);
            DrawText(rbuf, PX+175, y, 10, {120,120,120,255});
            if (clicked && CheckCollisionPointRec(mouse, {(float)(PX+10),(float)y,300,14})) {
                params.fluid = opts[i].ft;
                // Ajuste T_inlet et P par défaut selon fluide
                switch(opts[i].ft) {
                case FluidType::EAU:
                    if (params.T_inlet < 200) params.T_inlet = 286.0f;
                    if (params.P_bar < 10)    params.P_bar   = 155.0f;
                    params.convMode = ConvectionMode::FORCEE;
                    break;
                case FluidType::SODIUM:
                    params.T_inlet  = 400.0f;
                    params.P_bar    = 1.0f;
                    params.convMode = ConvectionMode::COMBINEE;
                    break;
                case FluidType::PLOMB_BI:
                    params.T_inlet  = 400.0f;
                    params.P_bar    = 1.0f;
                    params.convMode = ConvectionMode::FORCEE;
                    break;
                case FluidType::HELIUM:
                    params.T_inlet  = 250.0f;
                    params.P_bar    = 70.0f;
                    params.convMode = ConvectionMode::FORCEE;
                    break;
                }
                snprintf(bufT, sizeof(bufT), "%.0f", params.T_inlet);
                snprintf(bufP, sizeof(bufP), "%.0f", params.P_bar);
                changed = true;
            }
            y += 18;
        }
        y += 6;

        // ---- Mode convection ----
        DrawText("Mode convection :", PX+10, y, 12, {180,180,180,255});
        y += 18;
        struct ConvOpt { ConvectionMode cm; const char* label; const char* tip; };
        ConvOpt covs[3] = {
            { ConvectionMode::FORCEE,    "Forcee (pompe)",     "REP, RNR-Pb, RHT" },
            { ConvectionMode::NATURELLE, "Naturelle (thermo)", "Urgence"           },
            { ConvectionMode::COMBINEE,  "Combinee",           "RNR-Na (defaut)"   },
        };
        for (int i = 0; i < 3; ++i) {
            bool act = (params.convMode == covs[i].cm);
            DrawCircle(PX+20, y+6, 5, act ? Color{80,200,100,255} : Color{60,60,60,255});
            DrawCircleLines(PX+20, y+6, 5, LIGHTGRAY);
            DrawText(covs[i].label, PX+32, y, 12, LIGHTGRAY);
            DrawText(covs[i].tip, PX+175, y, 10, {100,100,100,255});
            if (clicked && CheckCollisionPointRec(mouse, {(float)(PX+10),(float)y,300,14})) {
                params.convMode = covs[i].cm;
                changed = true;
            }
            y += 16;
        }
        y += 6;

        // ---- Saisie T_inlet et P ----
        auto drawField = [&](const char* label, char* buf, bool& focus,
                              float& val, float vmin, float vmax,
                              int fx, int fy, int fw) {
            DrawText(label, fx, fy-14, 11, {160,160,160,255});
            Rectangle r = {(float)fx,(float)fy,(float)fw,22};
            bool hover = CheckCollisionPointRec(mouse, r);
            if (clicked && hover)  focus = true;
            if (clicked && !hover) {
                if (focus) { // valider
                    float v = (float)atof(buf);
                    if (v >= vmin && v <= vmax) val = v;
                    else snprintf(buf, 16, "%.0f", val);
                    changed = true;
                }
                focus = false;
            }
            DrawRectangleRec(r, focus ? Color{25,35,60,255} : Color{15,20,40,255});
            DrawRectangleLinesEx(r, 1, focus ? Color{80,180,255,255} : Color{60,80,120,255});
            DrawText(buf, fx+4, fy+4, 12, WHITE);
            if (focus && ((int)(GetTime()*2)%2==0)) {
                int tw = MeasureText(buf, 12);
                DrawRectangle(fx+4+tw, fy+3, 2, 15, WHITE);
            }
            if (focus) {
                int key = GetCharPressed();
                while (key > 0) {
                    int len = strlen(buf);
                    if (key == ',') key = '.';
                    if ((key>='0'&&key<='9')||key=='.') {
                        if (len < 15) { buf[len]=(char)key; buf[len+1]='\0'; }
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    int len = strlen(buf);
                    if (len > 0) buf[len-1] = '\0';
                }
                if (IsKeyPressed(KEY_ENTER)) {
                    float v = (float)atof(buf);
                    if (v >= vmin && v <= vmax) val = v;
                    else snprintf(buf, 16, "%.0f", val);
                    focus = false;
                    changed = true;
                }
            }
        };

        DrawText("Conditions d'entree :", PX+10, y, 12, {180,180,180,255});
        y += 18;
        drawField("T entree (°C)", bufT, focusT, params.T_inlet,
                   0.0f, 800.0f, PX+15, y, 120);
        drawField("Pression (bar)", bufP, focusP, params.P_bar,
                   0.1f, 300.0f, PX+155, y, 120);
        y += 36;

        // ---- Affichage mode ----
        const char* modeLabels[3] = {"Fleches","Overlay","Fleches+Couleur"};
        DrawText("Affichage [F] :", PX+10, y, 12, {180,180,180,255});
        DrawText(modeLabels[(int)displayMode], PX+130, y, 12, {80,200,255,255});
        y += 20;

        // ---- Activer/désactiver ----
        Rectangle btnToggle = {(float)(PX+10),(float)y,120,26};
        bool hoverT = CheckCollisionPointRec(mouse, btnToggle);
        DrawRectangleRec(btnToggle, active ?
            (hoverT ? Color{40,100,40,255} : Color{30,80,30,255}) :
            (hoverT ? Color{100,40,40,255} : Color{80,30,30,255}));
        DrawRectangleLinesEx(btnToggle,1,LIGHTGRAY);
        DrawText(active ? "  ACTIF" : "  INACTIF", PX+15, y+6, 12, WHITE);
        if (clicked && hoverT) { active = !active; changed = true; }

        // Bouton Appliquer
        Rectangle btnApply = {(float)(PX+140),(float)y,120,26};
        bool hoverA = CheckCollisionPointRec(mouse, btnApply);
        DrawRectangleRec(btnApply, hoverA ? Color{40,80,140,255} : Color{30,60,120,255});
        DrawRectangleLinesEx(btnApply,1,LIGHTGRAY);
        DrawText("  Appliquer", PX+145, y+6, 12, WHITE);
        if (clicked && hoverA) changed = true;

        // Fermer hors panneau
        Rectangle pr = {(float)PX,(float)PY,(float)PW,(float)PH};
        if (clicked && !CheckCollisionPointRec(mouse, pr)) visible = false;

        return changed;
    }

    // ================================================================
    //  RENDU FLUIDE dans la scène 3D (flèches) et en overlay 2D
    // ================================================================

    // ---- Flèches animées (BeginMode3D doit être actif) ----
    void draw3DArrows(const GridData& grid, const CoolantModel& model,
                      const RenderOptions& ropt) {
        if (!active) return;
        if (displayMode == CoolantDisplayMode::OVERLAY) return;

        arrowAnim += GetFrameTime() * 0.8f;
        if (arrowAnim > 1.0f) arrowAnim -= 1.0f;

        float step    = grid.dims.width + grid.dims.spacing;
        float halfW   = grid.dims.width * 0.5f;
        float cubeH   = ropt.cubeHeight;

        // Flèches entre colonnes et au-dessus des assemblages
        for (const auto& cube : grid.cubes) {
            float v   = model.getVfluid(cube.row, cube.col_idx);
            float T_f = model.getTfluid(cube.row, cube.col_idx);

            // Couleur selon vitesse (bleu lent → cyan rapide)
            float v_norm = fminf(v / 2.0f, 1.0f);
            Color arrowCol = {
                (unsigned char)(20),
                (unsigned char)(120 + (int)(135*v_norm)),
                (unsigned char)(200 + (int)(55*v_norm)),
                200
            };

            // Position de la flèche : à droite de l'assemblage, montant
            float x = cube.pos.x + halfW + step * 0.15f;
            float z = cube.pos.z;

            // 3 segments animés le long de la hauteur
            for (int seg = 0; seg < 3; ++seg) {
                float phase = fmodf(arrowAnim + seg / 3.0f, 1.0f);
                float yBase = -cubeH * 0.5f + phase * cubeH * 1.2f;

                Vector3 bot = {x, yBase,        z};
                Vector3 top = {x, yBase + 0.04f, z};
                DrawLine3D(bot, top, arrowCol);

                // Pointe de flèche
                float yw = yBase + 0.04f;
                DrawLine3D({x,        yw, z}, {x-0.015f, yw-0.02f, z}, arrowCol);
                DrawLine3D({x,        yw, z}, {x+0.015f, yw-0.02f, z}, arrowCol);
            }
        }
    }

    // ---- Overlay 2D température fluide (coin bas-gauche) ----
    void draw2DOverlay(const GridData& grid, const CoolantModel& model,
                       int sw, int sh) {
        if (!active) return;
        if (displayMode == CoolantDisplayMode::FLECHES) return;

        int cellPx = 28;
        int panelW = grid.cols * cellPx + 2;
        int panelH = grid.rows * cellPx + 30;
        int panelX = 10;
        int panelY = sh - panelH - 10;

        DrawRectangle(panelX, panelY, panelW, panelH, {5,10,20,220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {80,180,220,255});
        DrawText("FLUIDE T(°C)", panelX+4, panelY+4, 10, {80,200,255,255});

        // T min/max fluide pour normalisation
        float Tf_min = model.params.T_inlet;
        float Tf_max = model.params.T_inlet + 1.0f;
        for (int r = 0; r < grid.rows; ++r)
            for (int c = 0; c < grid.cols; ++c) {
                float t = model.getTfluid(r, c);
                if (t < Tf_min) Tf_min = t;
                if (t > Tf_max) Tf_max = t;
            }

        int mapY = panelY + 20;
        for (int r = 0; r < grid.rows; ++r) {
            for (int c = 0; c < grid.cols; ++c) {
                int px = panelX + c * cellPx;
                int py = mapY   + r * cellPx;
                float T_f = model.getTfluid(r, c);
                float norm = (Tf_max > Tf_min) ?
                    (T_f - Tf_min) / (Tf_max - Tf_min) : 0.0f;
                // Colormap bleu→cyan→blanc pour fluide
                Color col = {
                    (unsigned char)(norm * 80),
                    (unsigned char)(100 + norm * 155),
                    (unsigned char)(200 + norm * 55),
                    200
                };
                DrawRectangle(px, py, cellPx, cellPx, col);
                DrawRectangleLines(px, py, cellPx, cellPx, {0,0,0,60});

                // Vitesse comme texte si assez grand
                if (cellPx >= 24) {
                    char vbuf[8];
                    snprintf(vbuf, sizeof(vbuf), "%.1f", model.getVfluid(r, c));
                    DrawText(vbuf, px+2, py+cellPx/2-4, 8, WHITE);
                }
            }
        }

        // Légende T
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", Tf_min);
        DrawText(buf, panelX+2, panelY+panelH-10, 9, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "%.0f C", Tf_max);
        DrawText(buf, panelX+panelW-40, panelY+panelH-10, 9, LIGHTGRAY);
    }

    // ---- Overlay texte info fluide (coin haut-droit) ----
    void drawInfoOverlay(const CoolantModel& model, int sw) {
        if (!active) return;
        int x = sw - 240, y = 10;
        DrawRectangle(x, y, 230, 80, {0,0,0,170});
        DrawRectangleLines(x, y, 230, 80, {80,180,220,255});
        DrawText("CALOPORTEUR", x+8, y+8, 13, {80,200,255,255});
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", CoolantModel::fluidName(model.params.fluid));
        DrawText(buf, x+8, y+24, 11, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "T_in=%.0fC  P=%.0fbar",
                 model.params.T_inlet, model.params.P_bar);
        DrawText(buf, x+8, y+40, 11, LIGHTGRAY);
        const char* modeStr =
            model.params.convMode==ConvectionMode::FORCEE    ? "Convection forcee"   :
            model.params.convMode==ConvectionMode::NATURELLE ? "Convection naturelle" :
                                                               "Convection combinee";
        DrawText(modeStr, x+8, y+56, 11, {120,200,120,255});
    }
};