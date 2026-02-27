#pragma once
#include <raylib.h>
#include <string>
#include <vector>

// Un assemblage dans la grille 3D
struct Cube {
    Vector3 pos;          // position 3D (centrée sur la grille)
    Color   col;          // couleur affichée (colormap ou type)
    char    sym;          // symbole ASCII : A, B, C, X...
    float   temperature;  // °C
    float   flux;         // flux neutronique normalisé [0..1]
    int     row, col_idx; // indices dans la grille source
};

// Dimensions physiques des assemblages (en mètres)
struct AssemblyDims {
    float width   = 0.21f;  // largeur X (m)
    float height  = 4.00f;  // hauteur Y (m)
    float depth   = 0.21f;  // profondeur Z (m)
    float spacing = 0.01f;  // écart entre assemblages (m)
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

    void updateTempRange() {
        if (cubes.empty()) return;
        tempMin = tempMax = cubes[0].temperature;
        for (auto& c : cubes) {
            if (c.temperature < tempMin) tempMin = c.temperature;
            if (c.temperature > tempMax) tempMax = c.temperature;
        }
    }

    // Recalcule les positions 3D depuis les indices row/col et les dims
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
