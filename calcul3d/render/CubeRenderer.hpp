#pragma once
#include <raylib.h>
#include "../core/GridData.hpp"
#include "ColorMap.hpp"

struct RenderOptions {
    bool  showWires    = true;
    bool  showGrid     = true;
    bool  colormapMode = false;
    float cubeSize     = 0.88f;
    float cubeHeight   = 0.88f;
};

inline void updateRenderOptions(RenderOptions& opt) {
    if (IsKeyPressed(KEY_T)) opt.colormapMode = !opt.colormapMode;
    if (IsKeyPressed(KEY_W)) opt.showWires    = !opt.showWires;
    if (IsKeyPressed(KEY_G)) opt.showGrid     = !opt.showGrid;
    if (IsKeyPressed(KEY_UP))    opt.cubeHeight = fminf(opt.cubeHeight + 0.1f, 4.0f);
    if (IsKeyPressed(KEY_DOWN))  opt.cubeHeight = fmaxf(opt.cubeHeight - 0.1f, 0.1f);
    if (IsKeyPressed(KEY_RIGHT)) opt.cubeSize   = fminf(opt.cubeSize   + 0.05f, 0.98f);
    if (IsKeyPressed(KEY_LEFT))  opt.cubeSize   = fmaxf(opt.cubeSize   - 0.05f, 0.2f);
}

inline void drawGrid3D(const GridData& grid, const RenderOptions& opt) {
    if (opt.showGrid) DrawGrid(40, 1.0f);

    for (const auto& cube : grid.cubes) {
        Vector3 p = { cube.pos.x, opt.cubeHeight * 0.5f, cube.pos.z };

        // Mode colormap température OU couleur par type (cube.col)
        Color col = opt.colormapMode
            ? jetColor(normaliserTemp(cube.temperature, grid.tempMin, grid.tempMax))
            : cube.col;  // ← utilise la couleur stockée dans le cube

        DrawCube(p, opt.cubeSize, opt.cubeHeight, opt.cubeSize, col);
        if (opt.showWires)
            DrawCubeWires(p, opt.cubeSize, opt.cubeHeight, opt.cubeSize, {0,0,0,140});
    }
}
