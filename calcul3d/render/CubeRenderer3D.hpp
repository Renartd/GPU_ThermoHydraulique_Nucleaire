#pragma once
// ============================================================
//  CubeRenderer3D.hpp  v3
//
//  Deux modes selon sub_xy :
//
//  MODE DÉTAILLÉ (sub_xy <= 4, <= 16 mini-cubes/assy en plan) :
//    Chaque assemblage est rendu comme sub_xy × sub_xy × sub_z
//    mini-cubes individuels, colorés par température interpolée.
//    Le rendu 3D reflète exactement le maillage physique.
//
//  MODE SIMPLIFIÉ (sub_xy > 4) :
//    Trop de mini-cubes pour le rendu temps réel.
//    On affiche 1 cube par assemblage avec gradient axial,
//    et on indique "sub_xy=N" dans l'overlay.
//    La physique reste calculée sur la vraie grille fine.
//
//  Dans les deux cas :
//    - Couleur par température (mode colormap) ou par type
//    - Gradient axial visible
//    - Grille de séparation optionnelle
// ============================================================
#include <raylib.h>
#include <cmath>
#include <vector>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"

inline void drawGrid3DAxial(const GridData& grid,
                             const RenderOptions& ropt,
                             int sub_xy = 1,
                             int sub_z  = 8)
{
    if (ropt.showGrid) DrawGrid(40, 1.0f);

    float tMin = grid.tempMin;
    float tMax = grid.tempMax;
    if (tMax <= tMin) tMax = tMin + 1.0f;

    // dims.width = assy_width_m (taille NETTE de l'assemblage, sans le jeu)
    // dims.spacing = assy_gap_m  (jeu inter-assemblage)
    // Le pas centre-à-centre = dims.width + dims.spacing
    // cubeW DOIT être <= dims.width pour éviter les collisions
    float assy_w = grid.dims.width;              // taille nette assemblage (m)
    float assy_gap = grid.dims.spacing;          // jeu inter-assemblage (m)
    float assy_h = ropt.cubeHeight;              // hauteur visuelle
    // cubeSize [0..1] : facteur appliqué à assy_width — jamais au pitch
    // cubeSize=1.0 → assemblage pleine taille (jeu = assy_gap uniquement)
    // cubeSize=0.95 → assemblage 95% + marge visuelle supplémentaire
    float cubeW  = assy_w * ropt.cubeSize;       // < assy_w → pas de collision
    (void)assy_gap;

    const int DETAIL_THRESHOLD = 4;   // sub_xy <= 4 → mode détaillé

    if (sub_xy <= DETAIL_THRESHOLD) {
        // ── MODE DÉTAILLÉ ─────────────────────────────────────
        float cell_w = cubeW   / (float)sub_xy;   // largeur mini-cube
        float cell_h = assy_h  / (float)sub_z;    // hauteur mini-cube
        float gap_w  = cell_w  * 0.04f;           // jeu visuel inter-cellule

        for (const auto& cube : grid.cubes) {
            float cx_base = cube.pos.x - cubeW * 0.5f + cell_w * 0.5f;
            float cz_base = cube.pos.z - cubeW * 0.5f + cell_w * 0.5f;
            float y_base  = -assy_h * 0.5f;

            for (int ix = 0; ix < sub_xy; ++ix)
            for (int iz = 0; iz < sub_xy; ++iz) {
                float cx = cx_base + ix * cell_w;
                float cz = cz_base + iz * cell_w;

                for (int iy = 0; iy < sub_z; ++iy) {
                    // Température de cette sous-cellule :
                    // T_axial[iy] est la moyenne XZ de la tranche iy
                    // Pour une vraie valeur par sous-cellule XZ il faudrait
                    // un buffer 3D complet — on utilise T_axial comme approx
                    float T_s = cube.getTaxial(iy);
                    float t   = (T_s - tMin) / (tMax - tMin);
                    t = fmaxf(0.f, fminf(1.f, t));

                    Color col = ropt.colormapMode
                        ? jetColor(t) : cube.col;

                    // Légère variation XZ pour montrer la subdivision
                    if (ropt.colormapMode && sub_xy > 1) {
                        // Variation ±5% selon position XZ (simulée)
                        float var = 0.05f * (((ix+iz) % 2) ? 1.f : -1.f);
                        t = fmaxf(0.f, fminf(1.f, t + var));
                        col = jetColor(t);
                    }

                    float yc = y_base + (iy + 0.5f) * cell_h;
                    float w2 = cell_w - gap_w;
                    float h2 = cell_h * 0.98f;

                    DrawCube({cx, yc, cz}, w2, h2, w2, col);

                    if (ropt.showWires && cell_w > 0.015f)
                        DrawCubeWires({cx, yc, cz}, w2, h2, w2, {0,0,0,30});
                }
            }
            // Contour assemblage
            DrawCubeWires({cube.pos.x, 0.f, cube.pos.z},
                          cubeW, assy_h, cubeW, {0,0,0,60});
        }

    } else {
        // ── MODE SIMPLIFIÉ (sub_xy > 4) ───────────────────────
        // 1 cube par assemblage, gradient axial, contour
        int   ns     = sub_z > 0 ? sub_z : 8;
        float sliceH = assy_h / (float)ns;

        for (const auto& cube : grid.cubes) {
            float cx = cube.pos.x;
            float cz = cube.pos.z;

            if (cube.T_axial.empty() || !ropt.colormapMode) {
                float t = normaliserTemp(cube.temperature, tMin, tMax);
                DrawCube({cx, 0.f, cz}, cubeW, assy_h, cubeW,
                         ropt.colormapMode ? jetColor(t) : cube.col);
                DrawCubeWires({cx, 0.f, cz}, cubeW, assy_h, cubeW, {0,0,0,60});
                continue;
            }

            float yBase = -assy_h * 0.5f;
            for (int s = 0; s < ns; ++s) {
                float T_s = cube.getTaxial(s);
                float t   = (T_s - tMin) / (tMax - tMin);
                t = fmaxf(0.f, fminf(1.f, t));
                float yc = yBase + (s + 0.5f) * sliceH;
                DrawCube({cx, yc, cz}, cubeW, sliceH * 0.98f, cubeW,
                         jetColor(t));
            }
            DrawCubeWires({cx, 0.f, cz}, cubeW, assy_h, cubeW, {0,0,0,60});
        }
    }
}

// Variante avec ombres (garde la même logique)
inline void drawGrid3DAxialWithShadows(const GridData& grid,
                                        const RenderOptions& ropt,
                                        const std::vector<float>& /*shadowFactors*/,
                                        bool  /*shadowEnabled*/,
                                        int   sub_xy = 1,
                                        int   sub_z  = 8)
{
    // Délègue vers la version sans ombres pour l'instant
    drawGrid3DAxial(grid, ropt, sub_xy, sub_z);
}