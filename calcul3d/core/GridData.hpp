#pragma once
// ============================================================
//  GridData.hpp  v2.0
//
//  NOUVEAUTÉS v2 :
//    - Maillage piloté par h_target (taille de maille cible)
//      → cols/rows/slices CALCULÉS depuis les dimensions physiques
//    - Rapport d'aspect vérifié automatiquement
//    - MeshConfig exposé pour l'UI (DimsPanel)
//    - dx/dy/dz RÉELS stockés dans GridData (plus de suppositions)
//    - Total 3D : total3d = cols × rows × slices
//    - Cube reste l'unité de rendu 3D ; les données physiques
//      neutroniques/thermiques sont sur la grille 3D étendue
// ============================================================
#include <raylib.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

enum class NodeZone { FUEL, MODERATOR, REFLECTOR, CONTROL_ROD, COOLANT, EMPTY };

// ---------------------------------------------------------------
//  Cube : unité de rendu (1 cube = 1 assemblage)
// ---------------------------------------------------------------
struct Cube {
    Vector3 pos;
    Color   col;
    char    sym;
    float   temperature = 286.0f;
    float   flux        = 0.0f;
    int     row, col_idx;
    std::vector<float> T_axial;   // [nSlices] température axiale

    float getTaxial(int s) const {
        if (T_axial.empty()) return temperature;
        s = std::max(0, std::min(s, (int)T_axial.size()-1));
        return T_axial[s];
    }
};

struct ZoneNode {
    int      row, col;
    NodeZone zone    = NodeZone::EMPTY;
    float    param   = 1.0f;   // rho_rel pour mod, insertFraction pour barre
    Color    color   = {100,180,255,128};
    bool     visible = true;
};

// ---------------------------------------------------------------
//  AssemblyDims : dimensions physiques d'un assemblage (m)
// ---------------------------------------------------------------
struct AssemblyDims {
    float width   = 0.21f;   // m — largeur assemblage (REP 900MW ≈ 0.214m)
    float height  = 4.00f;   // m — hauteur active
    float depth   = 0.21f;   // m — profondeur assemblage
    float spacing = 0.01f;   // m — jeu inter-assemblage
};

// ---------------------------------------------------------------
//  MeshConfig : configuration du maillage pilotée par h_target
//
//  L'utilisateur fixe h_target_cm (taille de maille souhaitée).
//  cols/rows/slices sont DÉDUITS des dimensions physiques.
//  L'UI affiche le maillage résultant et le rapport d'aspect.
// ---------------------------------------------------------------
struct MeshConfig {
    // ── Dimensions physiques du CŒUR (m) ─────────────────────
    float core_width_m  = 3.40f;  // largeur totale cœur (ex: REP 900MW ≈ 3.4m)
    float core_depth_m  = 3.40f;  // profondeur totale
    float core_height_m = 4.00f;  // hauteur active (axial)

    // ── Taille de maille cible (m) ───────────────────────────
    // Exemples :
    //   0.21 → 1 cellule ≈ 1 assemblage REP (maillage grossier)
    //   0.10 → sous-assemblage (4 cellules par assemblage)
    //   0.40 → regroupement 4 assemblages
    float h_target_m = 0.21f;

    // ── Calculé automatiquement ───────────────────────────────
    int cols   = 1;
    int rows   = 1;
    int slices = 1;

    float dx = 0.21f;   // m — taille réelle cellule en X
    float dz = 0.21f;   // m — taille réelle cellule en Z
    float dy = 0.25f;   // m — taille réelle cellule en Y (axial)

    float aspect_ratio = 1.0f;   // max(dx/dy, ...) — doit rester < 3

    // ── Mise à jour à partir de h_target ─────────────────────
    void update() {
        cols   = std::max(1, (int)std::round(core_width_m  / h_target_m));
        rows   = std::max(1, (int)std::round(core_depth_m  / h_target_m));
        slices = std::max(1, (int)std::round(core_height_m / h_target_m));

        dx = core_width_m  / (float)cols;
        dz = core_depth_m  / (float)rows;
        dy = core_height_m / (float)slices;

        float mx = std::max({dx/dz, dz/dx, dx/dy, dy/dx, dz/dy, dy/dz});
        aspect_ratio = mx;
    }

    int total2d() const { return cols * rows; }
    int total3d() const { return cols * rows * slices; }

    bool aspectOK() const { return aspect_ratio < 3.0f; }

    // Préréglages communs
    void presetAssemblyREP()  { h_target_m = 0.214f; update(); }
    void presetSubAssembly()  { h_target_m = 0.107f; update(); }
    void presetCoarse()       { h_target_m = 0.428f; update(); }
};

// ---------------------------------------------------------------
//  GridData : données de la grille de simulation
// ---------------------------------------------------------------
struct GridData {
    std::vector<Cube>     cubes;       // assemblages actifs (rendu)
    std::vector<ZoneNode> zoneNodes;   // modérateur, réflecteur, barres

    // ── Topologie (issue de MeshConfig) ──────────────────────
    int   cols   = 0;
    int   rows   = 0;
    int   slices = 8;    // tranches axiales (pour ThermalCompute)
    float offsetX = 0.0f, offsetZ = 0.0f;

    // ── Dimensions réelles des cellules (m) ──────────────────
    float dx_m = 0.21f;
    float dy_m = 0.50f;
    float dz_m = 0.21f;

    // ── Dimensions physiques du cœur (m) ─────────────────────
    float core_width_m  = 3.40f;
    float core_height_m = 4.00f;
    float core_depth_m  = 3.40f;

    AssemblyDims dims;
    float tempMin = 280.0f, tempMax = 320.0f;

    // ── Application d'une MeshConfig ─────────────────────────
    void applyMesh(const MeshConfig& mc) {
        cols   = mc.cols;
        rows   = mc.rows;
        slices = mc.slices;
        dx_m   = mc.dx;
        dy_m   = mc.dy;
        dz_m   = mc.dz;
        core_width_m  = mc.core_width_m;
        core_height_m = mc.core_height_m;
        core_depth_m  = mc.core_depth_m;

        // Synchroniser AssemblyDims (utilisé par le rendu)
        dims.width   = mc.dx;
        dims.height  = mc.core_height_m;
        dims.depth   = mc.dz;
        dims.spacing = 0.002f;  // jeu visuel minimal
    }

    // ── Totaux ───────────────────────────────────────────────
    int total2d() const { return cols * rows; }
    int total3d() const { return cols * rows * slices; }

    // ── Mises à jour température ──────────────────────────────
    void updateTempRange() {
        if (cubes.empty()) return;
        tempMin = tempMax = cubes[0].temperature;
        for (auto& c : cubes) {
            if (c.temperature < tempMin) tempMin = c.temperature;
            if (c.temperature > tempMax) tempMax = c.temperature;
        }
    }

    void updateTempRangeAxial() {
        if (cubes.empty()) return;
        tempMin = tempMax = cubes[0].temperature;
        for (auto& c : cubes) {
            for (float t : c.T_axial) {
                if (t < tempMin) tempMin = t;
                if (t > tempMax) tempMax = t;
            }
            if (c.T_axial.empty()) {
                if (c.temperature < tempMin) tempMin = c.temperature;
                if (c.temperature > tempMax) tempMax = c.temperature;
            }
        }
    }

    // ── Reconstruction des positions 3D des cubes ─────────────
    //    Utilise dx_m / dz_m réels (plus la supposition width+spacing)
    void rebuildPositions() {
        offsetX = cols * dx_m * 0.5f - dx_m * 0.5f;
        offsetZ = rows * dz_m * 0.5f - dz_m * 0.5f;
        for (auto& cube : cubes) {
            cube.pos.x = cube.col_idx * dx_m - offsetX;
            cube.pos.z = cube.row     * dz_m - offsetZ;
        }
    }

    Vector3 cellPos(int row_, int col_) const {
        return { col_ * dx_m - offsetX, 0.0f, row_ * dz_m - offsetZ };
    }

    bool isFuel(int row_, int col_) const {
        for (const auto& c : cubes)
            if (c.row == row_ && c.col_idx == col_) return true;
        return false;
    }

    // ── Génération automatique des zones ──────────────────────
    void autoGenerateZones(int nReflectorRings = 1) {
        zoneNodes.clear();
        std::vector<std::vector<bool>> occ(rows, std::vector<bool>(cols, false));
        for (const auto& c : cubes) occ[c.row][c.col_idx] = true;

        // Modérateur : cellules vides adjacentes à un assemblage
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (occ[r][c]) continue;
                bool hasN = false;
                for (int dr = -1; dr <= 1 && !hasN; ++dr)
                for (int dc = -1; dc <= 1 && !hasN; ++dc) {
                    if (dr==0 && dc==0) continue;
                    int nr = r+dr, nc = c+dc;
                    if (nr>=0&&nr<rows&&nc>=0&&nc<cols&&occ[nr][nc]) hasN = true;
                }
                if (hasN) {
                    ZoneNode zn; zn.row=r; zn.col=c;
                    zn.zone=NodeZone::MODERATOR; zn.param=1.0f;
                    zn.color={80,160,255,100};
                    zoneNodes.push_back(zn);
                }
            }
        }

        // Réflecteur : couronnes externes
        for (int ring = 1; ring <= nReflectorRings; ++ring) {
            int r0=-ring, r1=rows-1+ring, c0=-ring, c1=cols-1+ring;
            for (int r=r0; r<=r1; ++r)
            for (int c=c0; c<=c1; ++c) {
                if (r>=0&&r<rows&&c>=0&&c<cols) continue;
                if (r==r0||r==r1||c==c0||c==c1) {
                    ZoneNode zn; zn.row=r; zn.col=c;
                    zn.zone=NodeZone::REFLECTOR; zn.param=1.0f;
                    zn.color={160,160,160,120};
                    zoneNodes.push_back(zn);
                }
            }
        }
    }

    void setControlRod(int row_, int col_, float insertFraction) {
        for (auto& zn : zoneNodes)
            if (zn.zone==NodeZone::CONTROL_ROD && zn.row==row_ && zn.col==col_) {
                zn.param=insertFraction; return;
            }
        ZoneNode zn; zn.row=row_; zn.col=col_;
        zn.zone=NodeZone::CONTROL_ROD; zn.param=insertFraction;
        zn.color={30,30,30,255};
        zoneNodes.push_back(zn);
    }
};