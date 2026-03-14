#pragma once
#include <raylib.h>
#include <cmath>

// ============================================================
//  Colormap "jet"
// ============================================================
inline Color jetColor(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float r, g, b;
    if      (t < 0.25f) { r=0.0f; g=t/0.25f; b=1.0f; }
    else if (t < 0.50f) { r=0.0f; g=1.0f; b=1.0f-(t-0.25f)/0.25f; }
    else if (t < 0.75f) { r=(t-0.5f)/0.25f; g=1.0f; b=0.0f; }
    else                { r=1.0f; g=1.0f-(t-0.75f)/0.25f; b=0.0f; }
    return {(unsigned char)(r*255),(unsigned char)(g*255),(unsigned char)(b*255),255};
}

inline float normaliserTemp(float temp, float tempMin, float tempMax) {
    if (tempMax <= tempMin) return 0.5f;
    return (temp - tempMin) / (tempMax - tempMin);
}

inline Color symbolColor(char sym) {
    switch (sym) {
        case 'A': return {255,200, 50,255};
        case 'B': return { 50,150,255,255};
        case 'C': return {100,220,100,255};
        case 'X': return {180, 80, 80,255};
        default:  return {200,200,200,255};
    }
}

// ============================================================
//  RenderOptions — défini UNE SEULE FOIS ici
// ============================================================
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
    if (IsKeyPressed(KEY_UP))    opt.cubeHeight = fminf(opt.cubeHeight+0.1f, 4.0f);
    if (IsKeyPressed(KEY_DOWN))  opt.cubeHeight = fmaxf(opt.cubeHeight-0.1f, 0.1f);
    if (IsKeyPressed(KEY_RIGHT)) opt.cubeSize   = fminf(opt.cubeSize+0.05f,  0.98f);
    if (IsKeyPressed(KEY_LEFT))  opt.cubeSize   = fmaxf(opt.cubeSize-0.05f,  0.2f);
}