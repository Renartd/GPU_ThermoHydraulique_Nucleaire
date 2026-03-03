#pragma once
// ============================================================
//  HeatmapPanel.hpp
//  Plan de coupe 2D : heatmap overlay de la grille thermique
//  Touche H : afficher/masquer
//  Position : coin BAS-DROITE (corrigé — était centré)
// ============================================================
#include <raylib.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"

struct HeatmapPanel {
    bool visible  = false;
    int  cellPx   = 28;
    bool showGrid = true;

    void draw(const GridData& grid, int sw, int sh) {

        if (IsKeyPressed(KEY_H)) visible = !visible;
        if (IsKeyPressed(KEY_KP_ADD))      cellPx = std::min(cellPx + 4, 64);
        if (IsKeyPressed(KEY_KP_SUBTRACT)) cellPx = std::max(cellPx - 4, 8);

        if (!visible) return;

        int cols = grid.cols;
        int rows = grid.rows;

        int mapW   = cols * cellPx;
        int mapH   = rows * cellPx;
        int panelW = mapW + 2;
        int panelH = mapH + 32;  // 20 titre + 12 légende

        // ---- BAS-DROITE ----
        int panelX = sw - panelW - 8;
        int panelY = sh - panelH - 8;

        DrawRectangle(panelX, panelY, panelW, panelH, {10,10,20,220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {80,120,180,255});
        DrawText("PLAN DE COUPE 2D", panelX+4, panelY+4, 11, LIGHTGRAY);

        int mapY = panelY + 20;

        // Températures à plat
        std::vector<float> T_flat(rows * cols, grid.tempMin);
        for (const auto& cube : grid.cubes) {
            int idx = cube.row * cols + cube.col_idx;
            if (idx >= 0 && idx < rows * cols)
                T_flat[idx] = cube.temperature;
        }

        std::vector<int> mask(rows * cols, 0);
        for (const auto& cube : grid.cubes)
            mask[cube.row * cols + cube.col_idx] = 1;

        float tMin = grid.tempMin, tMax = grid.tempMax;
        if (tMax <= tMin) tMax = tMin + 1.0f;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = r * cols + c;
                int px  = panelX + c * cellPx;
                int py  = mapY   + r * cellPx;

                if (mask[idx] == 0) {
                    DrawRectangle(px, py, cellPx, cellPx, {20,30,60,180});
                } else {
                    int sub = std::max(1, cellPx / 4);
                    for (int sy = 0; sy < cellPx; sy += sub) {
                        for (int sx = 0; sx < cellPx; sx += sub) {
                            float fx = (float)sx / cellPx;
                            float fy = (float)sy / cellPx;
                            float T  = bilinear(T_flat, mask, r, c,
                                                fx, fy, rows, cols, grid.tempMin);
                            float t  = (T - tMin) / (tMax - tMin);
                            t = fmaxf(0.0f, fminf(1.0f, t));
                            DrawRectangle(px+sx, py+sy,
                                std::min(sub, cellPx-sx),
                                std::min(sub, cellPx-sy),
                                jetColor(t));
                        }
                    }
                }
                if (showGrid && cellPx >= 12)
                    DrawRectangleLines(px, py, cellPx, cellPx, {0,0,0,80});
            }
        }

        // Légende
        int legY = mapY + mapH + 2;
        drawMiniLegend(panelX+2, legY, panelW-4, 7, tMin, tMax);
    }

private:
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

    void drawMiniLegend(int x, int y, int w, int h, float tMin, float tMax) {
        for (int i = 0; i < w; ++i) {
            float t = (float)i / (float)(w-1);
            DrawRectangle(x+i, y, 1, h, jetColor(t));
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0fC", tMin);
        DrawText(buf, x, y+h+1, 9, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "%.0fC", tMax);
        int tw = MeasureText(buf, 9);
        DrawText(buf, x+w-tw, y+h+1, 9, LIGHTGRAY);
    }
};