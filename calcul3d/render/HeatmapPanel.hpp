#pragma once
// ============================================================
//  HeatmapPanel.hpp
//  Plan de coupe 2D : heatmap overlay de la grille thermique
//  Touche H : afficher/masquer
//  Interpolation bilinéaire entre nœuds pour un rendu lissé
// ============================================================
#include <raylib.h>
#include <vector>
#include <cmath>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"

struct HeatmapPanel {
    bool visible  = false;
    int  cellPx   = 32;    // pixels par cellule (zoom)
    bool showGrid = true;

    // --------------------------------------------------------
    //  Affiche la heatmap 2D en overlay (coin bas-droit)
    // --------------------------------------------------------
    void draw(const GridData& grid, int sw, int sh) {

        if (IsKeyPressed(KEY_H)) visible = !visible;
        if (IsKeyPressed(KEY_KP_ADD))       cellPx = std::min(cellPx + 4, 64);
        if (IsKeyPressed(KEY_KP_SUBTRACT))  cellPx = std::max(cellPx - 4, 8);

        if (!visible) return;

        int cols = grid.cols;
        int rows = grid.rows;

        int panelW = cols * cellPx + 2;
        int panelH = rows * cellPx + 30; // 30 px pour titre + légende
        int panelX = sw - panelW - 10;
        int panelY = sh - panelH - 10;

        // Fond
        DrawRectangle(panelX, panelY, panelW, panelH, {10,10,20,220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {80,120,180,255});
        DrawText("PLAN DE COUPE 2D", panelX+4, panelY+4, 11, LIGHTGRAY);

        int mapY = panelY + 20;

        // Grille plate → température par cellule
        // Les cellules vides reçoivent T_inlet
        std::vector<float> T_flat(rows * cols, grid.tempMin);
        for (const auto& cube : grid.cubes) {
            int idx = cube.row * cols + cube.col_idx;
            if (idx < rows * cols)
                T_flat[idx] = cube.temperature;
        }

        // Masque (1=assemblage)
        std::vector<int> mask(rows * cols, 0);
        for (const auto& cube : grid.cubes)
            mask[cube.row * cols + cube.col_idx] = 1;

        // Dessin cellule par cellule avec interpolation bilinéaire
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = r * cols + c;
                int px  = panelX + c * cellPx;
                int py  = mapY   + r * cellPx;

                if (mask[idx] == 0) {
                    // Vide = caloporteur (bleu foncé)
                    DrawRectangle(px, py, cellPx, cellPx, {20,30,60,180});
                } else {
                    // Interpolation bilinéaire sur les sous-pixels
                    int sub = std::max(1, cellPx / 4);
                    for (int sy = 0; sy < cellPx; sy += sub) {
                        for (int sx = 0; sx < cellPx; sx += sub) {
                            float fx = (float)sx / cellPx; // 0→1
                            float fy = (float)sy / cellPx;

                            // Température interpolée avec les voisins
                            float T = bilinear(T_flat, mask, r, c, fx, fy,
                                               rows, cols, grid.tempMin);
                            float t = normaliserTemp(T, grid.tempMin, grid.tempMax);
                            Color col = jetColor(t);

                            DrawRectangle(px+sx, py+sy,
                                          std::min(sub, cellPx-sx),
                                          std::min(sub, cellPx-sy), col);
                        }
                    }
                }

                // Grille
                if (showGrid && cellPx >= 12)
                    DrawRectangleLines(px, py, cellPx, cellPx, {0,0,0,80});
            }
        }

        // Mini légende colormap
        drawMiniLegend(panelX+4, mapY + rows*cellPx + 2,
                       panelW-8, 6, grid.tempMin, grid.tempMax);
    }

private:
    // Interpolation bilinéaire entre T[r,c] et ses voisins
    float bilinear(const std::vector<float>& T,
                   const std::vector<int>& mask,
                   int r, int c, float fx, float fy,
                   int rows, int cols, float T_def) {

        auto get = [&](int rr, int cc) -> float {
            if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) return T_def;
            int i = rr*cols+cc;
            return mask[i] ? T[i] : T_def;
        };

        float T00 = get(r,   c);
        float T10 = get(r,   c+1);
        float T01 = get(r+1, c);
        float T11 = get(r+1, c+1);

        return T00*(1-fx)*(1-fy) + T10*fx*(1-fy)
             + T01*(1-fx)*fy     + T11*fx*fy;
    }

    void drawMiniLegend(int x, int y, int w, int h,
                        float tMin, float tMax) {
        for (int i = 0; i < w; ++i) {
            float t = (float)i / (float)(w-1);
            DrawRectangle(x+i, y, 1, h, jetColor(t));
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0fC", tMin);
        DrawText(buf, x, y+h+1, 9, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "%.0fC", tMax);
        DrawText(buf, x+w-28, y+h+1, 9, LIGHTGRAY);
    }
};
