#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

enum class NodeZone { FUEL, MODERATOR, REFLECTOR, CONTROL_ROD, COOLANT, EMPTY };

struct Cube {
    Vector3 pos;
    Color   col;
    char    sym;
    float   temperature = 286.0f;
    float   flux        = 0.0f;
    int     row, col_idx;
    std::vector<float> T_axial;

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

struct AssemblyDims {
    float width=0.21f, height=4.00f, depth=0.21f, spacing=0.01f;
};

struct GridData {
    std::vector<Cube>     cubes;
    std::vector<ZoneNode> zoneNodes;

    int   rows=0, cols=0;
    float offsetX=0.0f, offsetZ=0.0f;
    AssemblyDims dims;
    float tempMin=280.0f, tempMax=320.0f;
    int   nSlices=8;

    void updateTempRange() {
        if (cubes.empty()) return;
        tempMin=tempMax=cubes[0].temperature;
        for (auto& c:cubes) {
            if (c.temperature<tempMin) tempMin=c.temperature;
            if (c.temperature>tempMax) tempMax=c.temperature;
        }
    }

    void updateTempRangeAxial() {
        if (cubes.empty()) return;
        tempMin=tempMax=cubes[0].temperature;
        for (auto& c:cubes) {
            for (float t:c.T_axial) {
                if (t<tempMin) tempMin=t;
                if (t>tempMax) tempMax=t;
            }
            if (c.T_axial.empty()) {
                if (c.temperature<tempMin) tempMin=c.temperature;
                if (c.temperature>tempMax) tempMax=c.temperature;
            }
        }
    }

    void rebuildPositions() {
        float step=dims.width+dims.spacing;
        offsetX=cols*step*0.5f-step*0.5f;
        offsetZ=rows*step*0.5f-step*0.5f;
        for (auto& cube:cubes) {
            cube.pos.x=cube.col_idx*step-offsetX;
            cube.pos.z=cube.row    *step-offsetZ;
        }
    }

    Vector3 cellPos(int row, int col) const {
        float step=dims.width+dims.spacing;
        return {col*step-offsetX, 0.0f, row*step-offsetZ};
    }

    bool isFuel(int row, int col) const {
        for (const auto& c:cubes)
            if (c.row==row&&c.col_idx==col) return true;
        return false;
    }

    // Genere modérateur (interstices) et réflecteur (couronnes externes)
    void autoGenerateZones(int nReflectorRings=1) {
        zoneNodes.clear();
        std::vector<std::vector<bool>> occ(rows,std::vector<bool>(cols,false));
        for (const auto& c:cubes) occ[c.row][c.col_idx]=true;

        // Modérateur : cellules vides avec au moins un voisin assemblage
        for (int r=0;r<rows;++r) {
            for (int c=0;c<cols;++c) {
                if (occ[r][c]) continue;
                bool hasN=false;
                for (int dr=-1;dr<=1&&!hasN;++dr)
                for (int dc=-1;dc<=1&&!hasN;++dc) {
                    if (dr==0&&dc==0) continue;
                    int nr=r+dr,nc=c+dc;
                    if (nr>=0&&nr<rows&&nc>=0&&nc<cols&&occ[nr][nc]) hasN=true;
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
        for (int ring=1;ring<=nReflectorRings;++ring) {
            int r0=-ring, r1=rows-1+ring, c0=-ring, c1=cols-1+ring;
            for (int r=r0;r<=r1;++r) for (int c=c0;c<=c1;++c) {
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

    void setControlRod(int row, int col, float insertFraction) {
        for (auto& zn:zoneNodes)
            if (zn.zone==NodeZone::CONTROL_ROD&&zn.row==row&&zn.col==col) {
                zn.param=insertFraction; return;
            }
        ZoneNode zn; zn.row=row; zn.col=col;
        zn.zone=NodeZone::CONTROL_ROD; zn.param=insertFraction;
        zn.color={30,30,30,255};
        zoneNodes.push_back(zn);
    }
};