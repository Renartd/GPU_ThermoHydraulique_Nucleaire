#pragma once
#include <raylib.h>
#include <cstdio>
#include "../core/ReactorParams.hpp"
#include "../core/GridData.hpp"
#include "ColorMap.hpp"
#include "CubeRenderer.hpp"

// Barre de colormap (dégradé horizontal bleu→rouge)
inline void drawColormapBar(int x, int y, int w, int h,
                             float tempMin, float tempMax) {
    // Dégradé
    for (int i = 0; i < w; ++i) {
        float t = (float)i / (float)(w - 1);
        Color c = jetColor(t);
        DrawRectangle(x + i, y, 1, h, c);
    }
    DrawRectangleLines(x, y, w, h, {150, 150, 150, 255});

    // Labels
    char bufMin[32], bufMax[32];
    snprintf(bufMin, sizeof(bufMin), "%.0fC", tempMin);
    snprintf(bufMax, sizeof(bufMax), "%.0fC", tempMax);
    DrawText(bufMin, x,         y + h + 4, 11, LIGHTGRAY);
    DrawText(bufMax, x + w - 35, y + h + 4, 11, LIGHTGRAY);
    DrawText("TEMPERATURE", x + w/2 - 42, y - 16, 12, LIGHTGRAY);
}

// Légende symboles
inline void drawLegendSymboles(int x, int y) {
    DrawRectangle(x, y, 200, 110, {0, 0, 0, 170});
    DrawRectangleLines(x, y, 200, 110, {80, 80, 80, 255});
    DrawText("LEGENDE TYPE", x + 10, y + 8, 13, LIGHTGRAY);
    struct { Color c; const char* label; } items[] = {
        {{255,200, 50,255}, "A - Combustible UOX"},
        {{ 50,150,255,255}, "B - MOX"},
        {{100,220,100,255}, "C - Faible enrichi."},
        {{180, 80, 80,255}, "X - Barre controle"},
    };
    for (int i = 0; i < 4; ++i) {
        DrawRectangle(x + 10, y + 28 + i*18, 12, 12, items[i].c);
        DrawText(items[i].label, x + 26, y + 29 + i*18, 12, LIGHTGRAY);
    }
}

// HUD principal
inline void drawHUD(int sw, int sh,
                    const ReactorParams& params,
                    const GridData& grid,
                    const RenderOptions& opt) {
    // --- Panneau paramètres (haut gauche) ---
    DrawRectangle(10, 10, 300, 148, {0, 0, 0, 170});
    DrawRectangleLines(10, 10, 300, 148, {80, 80, 80, 255});
    DrawText("PARAMETRES REACTEUR", 20, 18, 14, LIGHTGRAY);

    char buf[128];
    snprintf(buf, sizeof(buf), "Enrichissement : %.2f %%", params.enrichissement);
    DrawText(buf, 20, 40, 13, {255, 200, 50, 255});
    snprintf(buf, sizeof(buf), "Moderateur     : %.2f",    params.moderateur);
    DrawText(buf, 20, 58, 13, {100, 200, 255, 255});
    snprintf(buf, sizeof(buf), "Puissance      : %.1f %%", params.puissance);
    DrawText(buf, 20, 76, 13, {100, 255, 100, 255});
    snprintf(buf, sizeof(buf), "Temp. entree   : %.1f C",  params.tempEntree);
    DrawText(buf, 20, 94, 13, {150, 200, 255, 255});
    snprintf(buf, sizeof(buf), "Temp. sortie   : %.1f C",  params.tempSortie);
    DrawText(buf, 20,112, 13, {255, 150, 100, 255});
    snprintf(buf, sizeof(buf), "Assemblages    : %d",      (int)grid.cubes.size());
    DrawText(buf, 20,130, 13, LIGHTGRAY);

    // --- Colormap ou légende symboles (haut droit) ---
    if (opt.colormapMode) {
        drawColormapBar(sw - 260, 40, 240, 20, grid.tempMin, grid.tempMax);
    } else {
        drawLegendSymboles(sw - 210, 10);
    }

    // --- Mode actif ---
    const char* modeStr = opt.colormapMode ? "MODE : TEMPERATURE" : "MODE : TYPE";
    Color modeCol = opt.colormapMode ? Color{255,100,50,255} : Color{100,200,255,255};
    DrawText(modeStr, sw/2 - 70, 14, 14, modeCol);

    // --- Options affichage ---
    snprintf(buf, sizeof(buf), "Taille:%.2f  Hauteur:%.2f  Fils:%s  Grille:%s",
             opt.cubeSize, opt.cubeHeight,
             opt.showWires ? "ON" : "OFF",
             opt.showGrid  ? "ON" : "OFF");
    DrawText(buf, 10, sh - 48, 12, {130, 130, 130, 255});

    // --- Aide touches ---
    DrawText("T:mode  W:fils  G:grille  Fleches:taille/hauteur  Clic+glisser:rotation  Molette:zoom",
             10, sh - 28, 12, {100, 100, 100, 255});

    DrawFPS(sw - 60, sh - 20);
}
