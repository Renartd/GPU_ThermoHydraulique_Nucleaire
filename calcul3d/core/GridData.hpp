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
//
//  IMPORTANT : pitch = width + spacing (pas réseau centre-à-centre)
//  Le rendu utilise width pour la taille du cube,
//  et pitch pour l'espacement des centres → pas de collision.
// ---------------------------------------------------------------
struct AssemblyDims {
    float width   = 0.207f;  // m — largeur physique assemblage (< pitch)
    float height  = 4.00f;   // m — hauteur active
    float depth   = 0.207f;  // m — profondeur physique assemblage
    float spacing = 0.007f;  // m — jeu inter-assemblage
    // pitch = width + spacing (calculé, pas stocké)
    float pitch() const { return width + spacing; }
};

// ---------------------------------------------------------------
//  MeshConfig v2 : subdivision intra-assemblage
//
//  Chaque assemblage (pitch × height × pitch) est découpé en :
//    sub_xy × sub_xy  cellules dans le plan horizontal
//    sub_z            cellules dans la direction axiale
//
//  → La cellule élémentaire mesure :
//    dx = assy_pitch  / sub_xy   (m)
//    dz = assy_pitch  / sub_xy   (m)
//    dy = assy_height / sub_z    (m)
//
//  Maillage total :
//    cols_total   = n_assy_cols * sub_xy
//    rows_total   = n_assy_rows * sub_xy
//    slices_total = sub_z
//    total3d      = cols_total × rows_total × slices_total
//
//  sub_xy peut aller de 1 (1 cellule = 1 assemblage) à 2^16
//  Le GPU est nécessaire dès sub_xy > ~8 (>50k cellules)
// ---------------------------------------------------------------
struct MeshConfig {
    // ── Dimensions physiques d'un assemblage (m) ─────────────
    // assy_width_m  : taille NETTE de l'assemblage (boîtier seul)
    // assy_gap_m    : jeu inter-assemblage (espace vide entre deux boîtiers)
    // assy_pitch_m  : pas centre-à-centre = assy_width_m + assy_gap_m  (calculé)
    // assy_pitch_m  = assy_width_m + assy_gap_m  (pas réseau centre-à-centre)
    // assy_width_m  = dimension physique nette de l'assemblage (boîtier)
    // assy_gap_m    = jeu inter-assemblage (espace vide entre deux boîtiers)
    // COLLISION IMPOSSIBLE : les cubes font assy_width_m,
    //   les centres sont espacés de assy_pitch_m = width + gap
    float assy_width_m  = 0.2070f;  // m — largeur nette REP 900MW
    float assy_gap_m    = 0.0073f;  // m — jeu inter-assemblage REP 900MW
    float assy_height_m = 4.00f;    // m — hauteur active
    float assy_pitch_m  = 0.2143f;  // m — CALCULÉ : assy_width_m + assy_gap_m

    // ── Nombre d'assemblages dans la grille de chargement ────
    int n_assy_cols = 11;   // rempli depuis AssemblageLoader
    int n_assy_rows = 11;

    // ── Subdivisions intra-assemblage (saisie utilisateur) ───
    int sub_xy = 1;    // divisions en X et Z par assemblage (1..65536)
    int sub_z  = 8;    // divisions axiales (1..1024)

    // ── Calculé automatiquement ───────────────────────────────
    int cols   = 11;   // = n_assy_cols * sub_xy
    int rows   = 11;   // = n_assy_rows * sub_xy
    int slices = 8;    // = sub_z

    float dx = 0.214f;  // m — taille cellule en X
    float dz = 0.214f;  // m — taille cellule en Z
    float dy = 0.500f;  // m — taille cellule en Y

    float aspect_ratio = 1.0f;

    // ── Mise à jour ───────────────────────────────────────────
    void update() {
        sub_xy = std::max(1, sub_xy);
        sub_z  = std::max(1, sub_z);
        assy_gap_m   = std::max(0.001f, assy_gap_m);   // jeu minimum 1mm
        // Contrainte anti-collision : width < pitch (toujours respecté)
        assy_width_m = std::min(assy_width_m, assy_pitch_m - 0.001f);
        assy_pitch_m = assy_width_m + assy_gap_m;       // recalcul pitch

        cols   = n_assy_cols * sub_xy;
        rows   = n_assy_rows * sub_xy;
        slices = sub_z;

        dx = assy_pitch_m  / (float)sub_xy;  // pas physique cellule XZ
        dz = assy_pitch_m  / (float)sub_xy;
        dy = assy_height_m / (float)sub_z;   // pas physique cellule Y

        float mx = std::max({dx/dz, dz/dx, dx/dy, dy/dx, dz/dy, dy/dz});
        aspect_ratio = mx;
    }

    int  total2d() const { return cols * rows; }
    long long total3d() const { return (long long)cols * rows * slices; }

    bool aspectOK()    const { return aspect_ratio < 5.0f; }
    bool gpuRequired() const { return total3d() > 50000LL; }

    // Mémoire GPU estimée (Mo)
    float estimatedMemMB() const {
        // phi1, phi2, XS×9, zone, precursors×6 = ~18 float buffers
        return (float)total2d() * 18.0f * 4.0f / (1024.0f*1024.0f)
             + (float)total3d() * 2.0f  * 4.0f / (1024.0f*1024.0f);
    }

    // Dimensions physiques du cœur
    float core_width_m()  const { return n_assy_cols * assy_pitch_m; }
    float core_depth_m()  const { return n_assy_rows * assy_pitch_m; }
    float core_height_m() const { return assy_height_m; }

    // Préréglages
    void preset1x()  { sub_xy=1;   sub_z=8;   update(); }  // 1 cellule/assy
    void preset2x()  { sub_xy=2;   sub_z=16;  update(); }  // 4 cellules/assy
    void preset4x()  { sub_xy=4;   sub_z=32;  update(); }  // 16 cellules/assy
    void preset8x()  { sub_xy=8;   sub_z=64;  update(); }  // 64 cellules/assy
    void preset16x() { sub_xy=16;  sub_z=128; update(); }  // GPU requis
    void preset32x() { sub_xy=32;  sub_z=256; update(); }  // GPU requis
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
    // Le rendu reste à l'échelle assemblage (1 cube = 1 assemblage).
    // La physique (neutronique/thermique) utilise la grille subdivisée.
    void applyMesh(const MeshConfig& mc) {
        // Grille physique (subdivisée)
        cols   = mc.cols;    // = n_assy_cols * sub_xy
        rows   = mc.rows;    // = n_assy_rows * sub_xy
        slices = mc.slices;  // = sub_z
        dx_m   = mc.dx;
        dy_m   = mc.dy;
        dz_m   = mc.dz;
        core_width_m  = mc.core_width_m();
        core_height_m = mc.core_height_m();
        core_depth_m  = mc.core_depth_m();

        // Dimensions rendu = pitch assemblage (inchangé)
        dims.width   = mc.assy_width_m;    // taille NETTE — pas le pitch
        dims.height  = mc.assy_height_m;
        dims.depth   = mc.assy_width_m;
        dims.spacing = mc.assy_gap_m;       // jeu réel inter-assemblage
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
        // Les cubes sont positionnés à l'échelle ASSEMBLAGE
        // (dims.width = assy_pitch, indépendant de la subdivision physique)
        // Le pas est le PITCH (centre-à-centre), pas width+spacing
        // dims.width   = largeur nette  (taille visuelle du cube)
        // dims.spacing = jeu            (espace vide visible entre cubes)
        // step         = pitch = width + spacing (distance entre centres)
        float step = dims.width + dims.spacing;  // = assy_pitch_m
        int n_cols = 0, n_rows = 0;
        for (const auto& c : cubes) {
            n_cols = std::max(n_cols, c.col_idx + 1);
            n_rows = std::max(n_rows, c.row     + 1);
        }
        offsetX = n_cols * step * 0.5f - step * 0.5f;
        offsetZ = n_rows * step * 0.5f - step * 0.5f;
        for (auto& cube : cubes) {
            cube.pos.x = cube.col_idx * step - offsetX;
            cube.pos.z = cube.row     * step - offsetZ;
        }
    }

    Vector3 cellPos(int row_, int col_) const {
        // Le pas est le PITCH (centre-à-centre), pas width+spacing
        // dims.width   = largeur nette  (taille visuelle du cube)
        // dims.spacing = jeu            (espace vide visible entre cubes)
        // step         = pitch = width + spacing (distance entre centres)
        float step = dims.width + dims.spacing;  // = assy_pitch_m
        return { col_ * step - offsetX, 0.0f, row_ * step - offsetZ };
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