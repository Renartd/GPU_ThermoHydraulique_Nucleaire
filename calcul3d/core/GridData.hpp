#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

// Un assemblage dans la grille 3D
struct Cube {
    Vector3 pos;          // position 3D (centrée sur la grille)
    Color   col;          // couleur affichée
    char    sym;          // symbole ASCII : A, B, C, X...
    float   temperature;  // °C — moyenne sur les slices verticales
    float   flux;         // flux neutronique normalisé [0..1]
    int     row, col_idx; // indices dans la grille source

    // Profil thermique axial (bas→haut)
    // Taille = grid_slices, rempli par ThermalCompute::applyToGrid
    std::vector<float> T_axial;

    // Accès sécurisé à une tranche
    float getTaxial(int s) const {
        if (T_axial.empty()) return temperature;
        s = std::max(0, std::min(s, (int)T_axial.size()-1));
        return T_axial[s];
    }
};

// Dimensions physiques des assemblages (en mètres)
struct AssemblyDims {
    float width   = 0.21f;
    float height  = 4.00f;
    float depth   = 0.21f;
    float spacing = 0.01f;
};

// La grille complète
struct GridData {
    std::vector<Cube> cubes;
    int   rows    = 0;
    int   cols    = 0;
    float offsetX = 0.0f;
    float offsetZ = 0.0f;

    AssemblyDims dims;

    float tempMin = 280.0f;
    float tempMax = 320.0f;

    // Nombre de tranches verticales actuel (sync avec ThermalCompute)
    int nSlices = 8;

    void updateTempRange() {
        if (cubes.empty()) return;
        tempMin = tempMax = cubes[0].temperature;
        for (auto& c : cubes) {
            if (c.temperature < tempMin) tempMin = c.temperature;
            if (c.temperature > tempMax) tempMax = c.temperature;
        }
    }

    // T min/max global sur toutes les tranches
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

    void rebuildPositions() {
        float step = dims.width + dims.spacing;
        offsetX = cols * step * 0.5f - step * 0.5f;
        offsetZ = rows * step * 0.5f - step * 0.5f;
        for (auto& cube : cubes) {
            cube.pos.x = cube.col_idx * step - offsetX;
            cube.pos.z = cube.row     * step - offsetZ;
        }
    }
};