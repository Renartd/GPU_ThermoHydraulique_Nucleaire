#pragma once
// ============================================================
//  ModeratorRenderer.hpp
//  Rendu 3D : modérateurs semi-transparents, réflecteurs,
//  barres de contrôle (cylindres animés).
//  Anti-collision : ne dessine pas sur un assemblage combustible.
// ============================================================
#include <raylib.h>
#include <cmath>
#include "../core/GridData.hpp"
#include "CubeRenderer.hpp"

struct ModeratorRenderer {
    bool showModerator  = true;
    bool showReflector  = true;
    bool showControlRod = true;

    void draw(const GridData& grid, const RenderOptions& ropt, float animTime) {
        float step  = grid.dims.width + grid.dims.spacing;
        float cubeW = ropt.cubeSize  * step;
        float cubeH = ropt.cubeHeight;

        for (const auto& zn : grid.zoneNodes) {
            if (!zn.visible) continue;
            // Anti-collision : skip si combustible à cette position
            if (grid.isFuel(zn.row, zn.col)) continue;

            Vector3 pos = grid.cellPos(zn.row, zn.col);

            switch (zn.zone) {

            case NodeZone::MODERATOR:
                if (!showModerator) break;
                DrawCube(pos, cubeW*0.85f, cubeH*0.90f, cubeW*0.85f, zn.color);
                DrawCubeWires(pos, cubeW*0.85f, cubeH*0.90f, cubeW*0.85f, {100,160,255,40});
                break;

            case NodeZone::REFLECTOR:
                if (!showReflector) break;
                DrawCube(pos, cubeW*0.95f, cubeH*0.70f, cubeW*0.95f, zn.color);
                DrawCubeWires(pos, cubeW*0.95f, cubeH*0.70f, cubeW*0.95f, {120,120,120,60});
                break;

            case NodeZone::CONTROL_ROD: {
                if (!showControlRod) break;
                float ins    = zn.param;               // 0=sorti, 1=insere
                float rodH   = cubeH * ins;
                float yTop   =  cubeH * 0.5f;         // sommet du coeur
                float yCenter= yTop - rodH * 0.5f;    // centre de la partie inseree
                // Corps B4C
                DrawCylinder({pos.x,yCenter,pos.z}, cubeW*0.15f, cubeW*0.15f, rodH, 8, {20,20,20,240});
                // Marqueur rouge = position de la tete
                float yMark = yTop - rodH;
                DrawCylinder({pos.x,yMark,pos.z}, cubeW*0.20f, cubeW*0.20f, cubeH*0.03f, 8, {220,40,40,255});
                // Guide (ligne verticale)
                DrawLine3D({pos.x,-cubeH*0.5f,pos.z},{pos.x,cubeH*0.5f,pos.z},{80,80,80,100});
                break;
            }

            default: break;
            }
        }
    }
};