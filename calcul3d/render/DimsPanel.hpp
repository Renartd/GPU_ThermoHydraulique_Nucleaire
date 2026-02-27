#pragma once
// ============================================================
//  DimsPanel.hpp — Panneau de saisie cliquable (dimensions)
//  Touche P : ouvrir/fermer
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include "../core/GridData.hpp"

struct DimsPanel {
    bool visible    = false;
    int  activeField = -1;

    char bufWidth  [16] = "0.21";
    char bufHeight [16] = "4.00";
    char bufDepth  [16] = "0.21";
    char bufSpacing[16] = "0.01";

    void syncFromDims(const AssemblyDims& d) {
        snprintf(bufWidth,   sizeof(bufWidth),   "%.3f", d.width);
        snprintf(bufHeight,  sizeof(bufHeight),  "%.3f", d.height);
        snprintf(bufDepth,   sizeof(bufDepth),   "%.3f", d.depth);
        snprintf(bufSpacing, sizeof(bufSpacing), "%.3f", d.spacing);
    }

    // --------------------------------------------------------
    //  Appeler UNE SEULE FOIS par frame, entre BeginDrawing/EndDrawing
    //  Retourne true si les dims ont changé
    // --------------------------------------------------------
    bool update(AssemblyDims& dims, int sw) {

        // Toggle ouverture
        if (IsKeyPressed(KEY_P)) {
            visible = !visible;
            if (visible) { syncFromDims(dims); activeField = -1; }
        }

        if (!visible) return false;

        // --- Géométrie panneau ---
        const int PX = sw - 285, PY = 170;
        const int PW = 270,      PH = 230;

        // Fond
        DrawRectangle(PX, PY, PW, PH, {10, 10, 25, 235});
        DrawRectangleLines(PX, PY, PW, PH, {100, 140, 220, 255});
        DrawText("DIMENSIONS ASSEMBLAGE", PX+8, PY+8,  13, LIGHTGRAY);
        DrawText("(en metres)",           PX+8, PY+24, 11, {120,120,120,255});

        struct Field { const char* label; char* buf; } fields[4] = {
            {"Largeur  X (m) :", bufWidth},
            {"Hauteur  Y (m) :", bufHeight},
            {"Profond. Z (m) :", bufDepth},
            {"Espacement (m) :", bufSpacing},
        };

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // Désactiver si clic hors panneau
        Rectangle panelRect = {(float)PX, (float)PY, (float)PW, (float)PH};
        if (clicked && !CheckCollisionPointRec(mouse, panelRect))
            activeField = -1;

        for (int i = 0; i < 4; ++i) {
            int fy = PY + 46 + i * 38;
            DrawText(fields[i].label, PX+8, fy, 12, LIGHTGRAY);

            Rectangle box = {(float)(PX+8), (float)(fy+15), (float)(PW-20), 20};
            bool active = (activeField == i);

            DrawRectangleRec(box, active ? Color{25,35,70,255} : Color{18,18,38,255});
            DrawRectangleLinesEx(box, 1,
                active ? Color{80,160,255,255} : Color{70,70,110,255});
            DrawText(fields[i].buf, (int)box.x+5, (int)box.y+4, 12,
                active ? WHITE : Color{210,210,210,255});

            // Clic sur le champ → activer
            if (clicked && CheckCollisionPointRec(mouse, box))
                activeField = i;

            // Saisie clavier
            if (active) {
                int key = GetCharPressed();
                while (key > 0) {
                    int len = (int)strlen(fields[i].buf);
                    if ((key >= '0' && key <= '9') || key == '.') {
                        if (len < 14) {
                            fields[i].buf[len]   = (char)key;
                            fields[i].buf[len+1] = '\0';
                        }
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    int len = (int)strlen(fields[i].buf);
                    if (len > 0) fields[i].buf[len-1] = '\0';
                }
            }
        }

        // --- Bouton Appliquer ---
        Rectangle btnA = {(float)(PX+8), (float)(PY+PH-36), 115, 26};
        bool hoverA = CheckCollisionPointRec(mouse, btnA);
        DrawRectangleRec(btnA, hoverA ? Color{50,110,50,255} : Color{35,80,35,255});
        DrawRectangleLinesEx(btnA, 1, {90,190,90,255});
        DrawText("Appliquer [Entree]", (int)btnA.x+6, (int)btnA.y+6, 11, WHITE);

        bool apply = (clicked && hoverA) || IsKeyPressed(KEY_ENTER);
        bool changed = false;

        if (apply) {
            auto trySet = [](const char* buf, float& target) -> bool {
                float v = (float)atof(buf);
                if (v > 0.0f && v != target) { target = v; return true; }
                return false;
            };
            changed |= trySet(bufWidth,   dims.width);
            changed |= trySet(bufHeight,  dims.height);
            changed |= trySet(bufDepth,   dims.depth);
            changed |= trySet(bufSpacing, dims.spacing);
            if (changed) printf("[DimsPanel] Dims appliquées\n");
        }

        // --- Bouton Fermer ---
        Rectangle btnC = {(float)(PX+PW-88), (float)(PY+PH-36), 78, 26};
        bool hoverC = CheckCollisionPointRec(mouse, btnC);
        DrawRectangleRec(btnC, hoverC ? Color{110,35,35,255} : Color{80,25,25,255});
        DrawRectangleLinesEx(btnC, 1, {190,80,80,255});
        DrawText("Fermer", (int)btnC.x+14, (int)btnC.y+6, 12, WHITE);
        if (clicked && hoverC) visible = false;

        return changed;
    }
};
