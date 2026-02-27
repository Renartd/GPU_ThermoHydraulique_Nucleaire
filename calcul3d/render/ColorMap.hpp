#pragma once
#include <raylib.h>
#include <cmath>

// ============================================================
//  Colormap "jet" : bleu → cyan → vert → jaune → orange → rouge
//  t : valeur normalisée [0.0 = froid, 1.0 = chaud]
// ============================================================
inline Color jetColor(float t) {
    // Clamp
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float r, g, b;

    if (t < 0.25f) {
        // Bleu → Cyan
        r = 0.0f;
        g = t / 0.25f;
        b = 1.0f;
    } else if (t < 0.5f) {
        // Cyan → Vert
        r = 0.0f;
        g = 1.0f;
        b = 1.0f - (t - 0.25f) / 0.25f;
    } else if (t < 0.75f) {
        // Vert → Jaune/Orange
        r = (t - 0.5f) / 0.25f;
        g = 1.0f;
        b = 0.0f;
    } else {
        // Orange → Rouge
        r = 1.0f;
        g = 1.0f - (t - 0.75f) / 0.25f;
        b = 0.0f;
    }

    return {
        (unsigned char)(r * 255),
        (unsigned char)(g * 255),
        (unsigned char)(b * 255),
        255
    };
}

// Normalise une température entre tempMin et tempMax → [0..1]
inline float normaliserTemp(float temp, float tempMin, float tempMax) {
    if (tempMax <= tempMin) return 0.5f;
    return (temp - tempMin) / (tempMax - tempMin);
}

// Couleur par type d'assemblage (mode symbole)
inline Color symbolColor(char sym) {
    switch (sym) {
        case 'A': return {255, 200,  50, 255};  // or      — UOX standard
        case 'B': return { 50, 150, 255, 255};  // bleu    — MOX
        case 'C': return {100, 220, 100, 255};  // vert    — faible enrichissement
        case 'X': return {180,  80,  80, 255};  // rouge   — barre de contrôle
        default:  return {200, 200, 200, 255};  // gris
    }
}
