#pragma once
// ============================================================
//  HeatmapPanel.hpp  v2
//  Plan de coupe 2D — toujours n_assy_cols × n_assy_rows cases
//  quelle que soit la subdivision (sub_xy).
//
//  Chaque case = 1 assemblage, coloré par la moyenne de ses
//  sub_xy × sub_xy sous-cellules dans la grille physique.
//  Touche H : afficher/masquer
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

    // Appeler avec la MeshConfig courante pour connaître sub_xy
    void draw(const GridData& grid, const MeshConfig& mc, int sw, int sh) {

        if (IsKeyPressed(KEY_H)) visible = !visible;
        if (IsKeyPressed(KEY_KP_ADD))      cellPx = std::min(cellPx+4, 64);
        if (IsKeyPressed(KEY_KP_SUBTRACT)) cellPx = std::max(cellPx-4, 8);
        if (!visible) return;

        // Toujours afficher à l'échelle assemblage
        int assy_cols = mc.n_assy_cols;
        int assy_rows = mc.n_assy_rows;
        int sub       = mc.sub_xy;  // sous-cellules par assemblage en X/Z

        int mapW   = assy_cols * cellPx;
        int mapH   = assy_rows * cellPx;
        int panelW = mapW + 2;
        int panelH = mapH + 32;

        int panelX = sw - panelW - 8;
        int panelY = sh - panelH - 8;

        DrawRectangle(panelX, panelY, panelW, panelH, {10,10,20,220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {80,120,180,255});

        char title[48];
        snprintf(title, sizeof(title), "PLAN DE COUPE 2D  (%dx%d assy, sub=%d)",
                 assy_cols, assy_rows, sub);
        DrawText(title, panelX+4, panelY+4, 10, LIGHTGRAY);

        int mapY = panelY + 20;

        // ── Construire la grille température à l'échelle ASSEMBLAGE ──
        // Moyenne des sub×sub sous-cellules pour chaque assemblage
        int phys_cols = mc.cols;   // = assy_cols * sub
        int phys_rows = mc.rows;   // = assy_rows * sub

        // Température physique (grille subdivisée)
        std::vector<float> T_phys(phys_rows * phys_cols, grid.tempMin);
        std::vector<int>   has_phys(phys_rows * phys_cols, 0);

        for (const auto& cube : grid.cubes) {
            // cube.row / cube.col_idx sont dans l'espace ASSEMBLAGE
            // Les sous-cellules correspondantes sont dans [row*sub..(row+1)*sub)
            for (int dr = 0; dr < sub; ++dr)
            for (int dc = 0; dc < sub; ++dc) {
                int pr = cube.row     * sub + dr;
                int pc = cube.col_idx * sub + dc;
                int pi = pr * phys_cols + pc;
                if (pi >= 0 && pi < (int)T_phys.size()) {
                    T_phys[pi]  = cube.temperature;
                    has_phys[pi] = 1;
                }
            }
        }

        // Si ThermalCompute a mis à jour T_axial, utiliser la moyenne axiale
        // (déjà dans cube.temperature via applyToGrid)

        // Agréger par assemblage
        std::vector<float> T_assy(assy_rows * assy_cols, grid.tempMin);
        std::vector<int>   cnt_assy(assy_rows * assy_cols, 0);

        for (const auto& cube : grid.cubes) {
            int ai = cube.row * assy_cols + cube.col_idx;
            if (ai < 0 || ai >= assy_rows * assy_cols) continue;
            T_assy[ai]   += cube.temperature;
            cnt_assy[ai] += 1;
        }
        for (int i = 0; i < assy_rows * assy_cols; ++i)
            if (cnt_assy[i] > 0) T_assy[i] /= cnt_assy[i];

        // Masque assemblage
        std::vector<int> mask(assy_rows * assy_cols, 0);
        for (const auto& cube : grid.cubes)
            mask[cube.row * assy_cols + cube.col_idx] = 1;

        float tMin = grid.tempMin, tMax = grid.tempMax;
        if (tMax <= tMin) tMax = tMin + 1.0f;

        // ── Dessin ────────────────────────────────────────────
        for (int r = 0; r < assy_rows; ++r) {
            for (int c = 0; c < assy_cols; ++c) {
                int ai = r * assy_cols + c;
                int px = panelX + c * cellPx;
                int py = mapY   + r * cellPx;

                if (!mask[ai]) {
                    DrawRectangle(px, py, cellPx, cellPx, {20,30,60,180});
                } else {
                    // Dégradé bilinéaire intra-case pour rendu lisse
                    int step = std::max(1, cellPx / 8);
                    for (int sy = 0; sy < cellPx; sy += step) {
                        for (int sx = 0; sx < cellPx; sx += step) {
                            float fx = (float)sx / cellPx;
                            float fy = (float)sy / cellPx;
                            float T  = _bilinear(T_assy, mask,
                                                 r, c, fx, fy,
                                                 assy_rows, assy_cols,
                                                 grid.tempMin);
                            float t  = (T - tMin) / (tMax - tMin);
                            t = fmaxf(0.f, fminf(1.f, t));
                            int w2 = std::min(step, cellPx-sx);
                            int h2 = std::min(step, cellPx-sy);
                            DrawRectangle(px+sx, py+sy, w2, h2, jetColor(t));
                        }
                    }
                }
                if (showGrid && cellPx >= 10)
                    DrawRectangleLines(px, py, cellPx, cellPx, {0,0,0,80});
            }
        }

        // Légende
        int legY = mapY + mapH + 2;
        _drawLegend(panelX+2, legY, panelW-4, 7, tMin, tMax);
    }

private:
    float _bilinear(const std::vector<float>& T,
                    const std::vector<int>& mask,
                    int r, int c, float fx, float fy,
                    int rows, int cols, float T_def) {
        auto get = [&](int rr, int cc) -> float {
            if (rr<0||rr>=rows||cc<0||cc>=cols) return T_def;
            int i=rr*cols+cc;
            return mask[i] ? T[i] : T_def;
        };
        float T00=get(r,c), T10=get(r,c+1);
        float T01=get(r+1,c), T11=get(r+1,c+1);
        return T00*(1-fx)*(1-fy) + T10*fx*(1-fy)
             + T01*(1-fx)*fy     + T11*fx*fy;
    }

    void _drawLegend(int x, int y, int w, int h, float tMin, float tMax) {
        for (int i=0; i<w; ++i)
            DrawRectangle(x+i, y, 1, h, jetColor((float)i/(w-1)));
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0fC", tMin);
        DrawText(buf, x, y+h+1, 9, LIGHTGRAY);
        snprintf(buf, sizeof(buf), "%.0fC", tMax);
        DrawText(buf, x+w-MeasureText(buf,9), y+h+1, 9, LIGHTGRAY);
    }
};