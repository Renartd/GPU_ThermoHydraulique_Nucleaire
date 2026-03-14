#pragma once
// ============================================================
//  CubeRenderer3D.hpp
//  Rendu des assemblages avec gradient thermique vertical
//  Remplace drawGrid3D/drawGrid3DWithShadows pour le mode 3D
//
//  Chaque cube est découpé en N bandes horizontales colorées
//  selon T_axial[s], donnant un dégradé bas→haut visible.
// ============================================================
#include <raylib.h>
#include <cmath>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"

// ---- Rendu grille avec gradient axial (sans ombres) ----
inline void drawGrid3DAxial(const GridData& grid,
                             const RenderOptions& ropt) {
    if (ropt.showGrid) DrawGrid(40, 1.0f);
    float cubeH = ropt.cubeHeight;
    float cubeW = ropt.cubeSize;
    int   ns    = grid.slices > 0 ? grid.slices : 8;
    float sliceH = cubeH / (float)ns;

    float tMin = grid.tempMin;
    float tMax = grid.tempMax;
    if (tMax <= tMin) tMax = tMin + 1.0f;

    for (const auto& cube : grid.cubes) {
        float cx = cube.pos.x;
        float cz = cube.pos.z;
        float yBase = -cubeH * 0.5f;

        if (cube.T_axial.empty() || !ropt.colormapMode) {
            // Fallback : couleur uniforme
            float t = normaliserTemp(cube.temperature, tMin, tMax);
            DrawCube({cx, 0.0f, cz}, cubeW, cubeH, cubeW,
                     ropt.colormapMode ? jetColor(t) : cube.col);
            DrawCubeWires({cx, 0.0f, cz}, cubeW, cubeH, cubeW, {0,0,0,40});
            continue;
        }

        // Dessin slice par slice
        for (int s = 0; s < ns; ++s) {
            float T_s = cube.getTaxial(s);
            float t   = (T_s - tMin) / (tMax - tMin);
            t = fmaxf(0.0f, fminf(1.0f, t));
            Color col = jetColor(t);

            float yCenter = yBase + (s + 0.5f) * sliceH;
            DrawCube({cx, yCenter, cz}, cubeW, sliceH, cubeW, col);

            // Ligne de séparation fine tous les 4 slices
            if (s % 4 == 0 && ns > 4) {
                float yLine = yBase + s * sliceH;
                DrawLine3D({cx - cubeW*0.5f, yLine, cz - cubeW*0.5f},
                           {cx + cubeW*0.5f, yLine, cz - cubeW*0.5f},
                           {0,0,0,30});
            }
        }
        // Contour global du cube
        DrawCubeWires({cx, 0.0f, cz}, cubeW, cubeH, cubeW, {0,0,0,60});
    }
}

// ---- Rendu avec ombres RT + gradient axial ----
inline void drawGrid3DAxialWithShadows(const GridData& grid,
                                        const RenderOptions& ropt,
                                        const std::vector<float>& shadowFactors,
                                        bool shadowEnabled) {
    if (ropt.showGrid) DrawGrid(40, 1.0f);
    float cubeH  = ropt.cubeHeight;
    float cubeW  = ropt.cubeSize;
    int   ns     = grid.slices > 0 ? grid.slices : 8;
    float sliceH = cubeH / (float)ns;

    float tMin = grid.tempMin;
    float tMax = grid.tempMax;
    if (tMax <= tMin) tMax = tMin + 1.0f;

    for (int ci = 0; ci < (int)grid.cubes.size(); ++ci) {
        const auto& cube = grid.cubes[ci];
        float cx = cube.pos.x;
        float cz = cube.pos.z;
        float yBase = -cubeH * 0.5f;

        float shadow = (shadowEnabled && ci < (int)shadowFactors.size())
                       ? shadowFactors[ci] : 1.0f;

        if (cube.T_axial.empty() || !ropt.colormapMode) {
            float t   = normaliserTemp(cube.temperature, tMin, tMax);
            Color col = jetColor(t);
            col.r = (uint8_t)(col.r * shadow);
            col.g = (uint8_t)(col.g * shadow);
            col.b = (uint8_t)(col.b * shadow);
            DrawCube({cx, 0.0f, cz}, cubeW, cubeH, cubeW, col);
            DrawCubeWires({cx, 0.0f, cz}, cubeW, cubeH, cubeW, {0,0,0,40});
            continue;
        }

        for (int s = 0; s < ns; ++s) {
            float T_s = cube.getTaxial(s);
            float t   = (T_s - tMin) / (tMax - tMin);
            t = fmaxf(0.0f, fminf(1.0f, t));
            Color col = jetColor(t);
            // Ombre : plus sombre en bas (les slices du bas sont plus ombragées)
            float localShadow = shadow * (0.7f + 0.3f * (float)s / (float)(ns-1));
            col.r = (uint8_t)(col.r * localShadow);
            col.g = (uint8_t)(col.g * localShadow);
            col.b = (uint8_t)(col.b * localShadow);

            float yCenter = yBase + (s + 0.5f) * sliceH;
            DrawCube({cx, yCenter, cz}, cubeW, sliceH, cubeW, col);
        }
        DrawCubeWires({cx, 0.0f, cz}, cubeW, cubeH, cubeW, {0,0,0,60});
    }
}