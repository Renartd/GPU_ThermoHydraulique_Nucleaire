#pragma once
#include <raylib.h>
#include <vector>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"
#include "CubeRenderer.hpp"

inline Color applyShadow(Color col, float shadowFactor, float ambientLight = 0.55f) {
    float f = ambientLight + (1.0f - ambientLight) * shadowFactor;
    return {
        (unsigned char)(col.r * f),
        (unsigned char)(col.g * f),
        (unsigned char)(col.b * f),
        col.a
    };
}

inline void drawGrid3DWithShadows(const GridData& grid,
                                   const RenderOptions& opt,
                                   const std::vector<float>& shadowFactors,
                                   bool rtEnabled) {
    if (opt.showGrid) DrawGrid(40, 1.0f);

    for (int i = 0; i < (int)grid.cubes.size(); ++i) {
        const auto& cube = grid.cubes[i];
        Vector3 p = { cube.pos.x, opt.cubeHeight * 0.5f, cube.pos.z };

        Color col = opt.colormapMode
            ? jetColor(normaliserTemp(cube.temperature, grid.tempMin, grid.tempMax))
            : cube.col;  // ← couleur par type

        if (rtEnabled && i < (int)shadowFactors.size())
            col = applyShadow(col, shadowFactors[i]);

        DrawCube(p, opt.cubeSize, opt.cubeHeight, opt.cubeSize, col);
        if (opt.showWires)
            DrawCubeWires(p, opt.cubeSize, opt.cubeHeight, opt.cubeSize, {0,0,0,140});
    }
}
