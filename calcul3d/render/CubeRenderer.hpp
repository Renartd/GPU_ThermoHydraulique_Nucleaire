#pragma once
// ============================================================
//  CubeRenderer.hpp — rendu 3D simple (vue de dessus)
//  RenderOptions est défini dans ColorMap.hpp
// ============================================================
#include <raylib.h>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"

// Rendu 3D basique (cubes plats, vue de dessus)
inline void drawGrid3D(const GridData& grid, const RenderOptions& opt) {
    if (opt.showGrid) DrawGrid(40, 1.0f);

    for (const auto& cube : grid.cubes) {
        Vector3 p = { cube.pos.x, opt.cubeHeight * 0.5f, cube.pos.z };
        Color col = opt.colormapMode
            ? jetColor(normaliserTemp(cube.temperature, grid.tempMin, grid.tempMax))
            : cube.col;
        DrawCube(p, opt.cubeSize, opt.cubeHeight, opt.cubeSize, col);
        if (opt.showWires)
            DrawCubeWires(p, opt.cubeSize, opt.cubeHeight, opt.cubeSize, {0,0,0,140});
    }
}