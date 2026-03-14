#pragma once
// ============================================================
//  DimsPanel.hpp  v2.0
//
//  Panneau UI de configuration du maillage.
//  Touche D : ouvrir/fermer
//
//  Permet à l'utilisateur de régler :
//    - Dimensions physiques du cœur (largeur, profondeur, hauteur)
//    - Taille de maille cible h_target
//  Affiche en retour :
//    - Nombre de cellules calculé (cols × rows × slices)
//    - Rapport d'aspect (doit rester < 3)
//    - Avertissement si h > assemblage ou rapport dégradé
//
//  Déclenche un callback onChange(MeshConfig) si le maillage change.
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <functional>
#include "../core/GridData.hpp"

struct DimsPanel {
    bool visible = false;

    // Buffers de saisie
    char bufWidth[16]  = "3.40";
    char bufDepth[16]  = "3.40";
    char bufHeight[16] = "4.00";
    char bufH[16]      = "0.21";

    int  activeField  = -1;   // champ en cours d'édition (0-3)

    // Callback déclenché si le maillage a changé
    std::function<void(const MeshConfig&)> onChange;

    MeshConfig current;

    void init(const MeshConfig& mc) {
        current = mc;
        _syncBuffers();
    }

    void _syncBuffers() {
        snprintf(bufWidth,  sizeof(bufWidth),  "%.2f", current.core_width_m);
        snprintf(bufDepth,  sizeof(bufDepth),  "%.2f", current.core_depth_m);
        snprintf(bufHeight, sizeof(bufHeight), "%.2f", current.core_height_m);
        snprintf(bufH,      sizeof(bufH),      "%.3f", current.h_target_m);
        current.update();
    }

    bool update(int sw, int sh) {
        if (IsKeyPressed(KEY_D)) { visible = !visible; activeField = -1; }
        if (!visible) return false;

        const int PW=420, PH=400;
        const int PX=sw/2-PW/2, PY=sh/2-PH/2;

        DrawRectangle(PX, PY, PW, PH, {10,10,25,245});
        DrawRectangleLines(PX, PY, PW, PH, {100,200,255,255});
        DrawText("CONFIGURATION DU MAILLAGE", PX+12, PY+10, 14, LIGHTGRAY);
        DrawLine(PX,PY+28,PX+PW,PY+28,{60,80,120,200});

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed  = false;
        int y = PY + 40;

        // ── Dimensions physiques ──────────────────────────────
        DrawText("Dimensions physiques du coeur (m)", PX+12, y, 12, {180,200,255,255});
        y += 20;

        changed |= _field("Largeur X :", bufWidth,  0, PX, y, sw, sh);
        y += 30;
        changed |= _field("Profondeur Z :", bufDepth,   1, PX, y, sw, sh);
        y += 30;
        changed |= _field("Hauteur Y (axial) :", bufHeight, 2, PX, y, sw, sh);
        y += 36;

        // ── Taille de maille ──────────────────────────────────
        DrawLine(PX+8,y,PX+PW-8,y,{40,60,100,180});
        y += 10;
        DrawText("Taille de maille cible (m)", PX+12, y, 12, {180,200,255,255});
        y += 20;
        changed |= _field("h_target :", bufH, 3, PX, y, sw, sh);
        y += 36;

        // ── Préréglages ───────────────────────────────────────
        DrawText("Preréglages :", PX+12, y, 11, {140,140,140,255});
        y += 18;
        auto btn=[&](const char* lbl, int bx, int by, int bw)->bool{
            Rectangle r={(float)bx,(float)by,(float)bw,22};
            bool ov=CheckCollisionPointRec(mouse,r);
            DrawRectangleRec(r, ov?Color{50,80,150,255}:Color{30,50,100,200});
            DrawRectangleLinesEx(r,1,{80,120,220,200});
            DrawText(lbl,bx+6,by+5,11,LIGHTGRAY);
            return clicked&&ov;
        };

        if (btn("Assemblage REP\n(0.21m)", PX+12, y, 118)) {
            snprintf(bufH,sizeof(bufH),"0.210"); changed=true;
        }
        if (btn("Demi-assy\n(0.107m)", PX+138, y, 108)) {
            snprintf(bufH,sizeof(bufH),"0.107"); changed=true;
        }
        if (btn("Grossier\n(0.42m)", PX+254, y, 90)) {
            snprintf(bufH,sizeof(bufH),"0.420"); changed=true;
        }
        if (btn("Fin (0.05m)", PX+352, y, 90)) {
            snprintf(bufH,sizeof(bufH),"0.050"); changed=true;
        }
        y += 40;

        // ── Application ──────────────────────────────────────
        if (changed) {
            _applyBuffers();
            if (onChange) onChange(current);
        }

        // ── Résultat calculé ──────────────────────────────────
        DrawLine(PX+8,y,PX+PW-8,y,{40,60,100,180});
        y += 8;

        char buf[128];
        snprintf(buf, sizeof(buf), "Maillage : %d x %d x %d = %d cellules",
                 current.cols, current.rows, current.slices,
                 current.total3d());
        DrawText(buf, PX+12, y, 12, {100,220,100,255});
        y += 18;

        snprintf(buf, sizeof(buf), "dx=%.3fm  dy=%.3fm  dz=%.3fm",
                 current.dx, current.dy, current.dz);
        DrawText(buf, PX+12, y, 11, {160,160,200,255});
        y += 16;

        // Rapport d'aspect
        Color aspectCol = current.aspectOK() ? Color{100,220,100,255}
                                              : Color{255,120,60,255};
        snprintf(buf, sizeof(buf), "Rapport d'aspect : %.2f  %s",
                 current.aspect_ratio,
                 current.aspectOK() ? "OK" : "DEGRADE (> 3) !");
        DrawText(buf, PX+12, y, 11, aspectCol);
        y += 16;

        // Avertissement cellules
        int N = current.total3d();
        if (N > 100000) {
            DrawText("! > 100 000 cellules - performances réduites",
                     PX+12, y, 11, {255,200,60,255});
            y += 14;
        }

        // Bouton fermer
        Rectangle closeR={(float)(PX+PW-60),(float)(PY+PH-34),55,26};
        bool closeOv=CheckCollisionPointRec(mouse,closeR);
        DrawRectangleRec(closeR,closeOv?Color{120,40,40,255}:Color{80,30,30,200});
        DrawRectangleLinesEx(closeR,1,{200,80,80,255});
        DrawText("Fermer",PX+PW-54,PY+PH-28,11,LIGHTGRAY);
        if (clicked && closeOv) visible=false;

        return changed;
    }

private:
    bool _field(const char* label, char* buf, int id,
                int PX, int y, int sw, int sh)
    {
        DrawText(label, PX+12, y+6, 11, {160,170,200,255});

        Rectangle r={(float)(PX+185),(float)y,110,22};
        bool isActive = (activeField==id);
        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        Color bgCol = isActive ? Color{30,50,90,255} : Color{20,30,60,200};
        Color bdCol = isActive ? Color{100,180,255,255} : Color{60,80,140,200};
        DrawRectangleRec(r, bgCol);
        DrawRectangleLinesEx(r, 1, bdCol);
        DrawText(buf, (int)r.x+5, (int)r.y+5, 11, LIGHTGRAY);

        if (clicked) {
            if (CheckCollisionPointRec(mouse, r)) activeField = id;
            else if (activeField==id)              activeField = -1;
        }

        // Édition clavier
        if (isActive) {
            int key = GetCharPressed();
            int len = (int)strlen(buf);
            while (key > 0) {
                if (len < 10 && (key>='0' && key<='9' || key=='.')) {
                    buf[len]=(char)key; buf[len+1]='\0';
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
                buf[len-1] = '\0';
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB)) {
                activeField = -1;
                return true;
            }
        }
        return false;
    }

    void _applyBuffers() {
        float w = (float)atof(bufWidth);
        float d = (float)atof(bufDepth);
        float hh= (float)atof(bufHeight);
        float h = (float)atof(bufH);

        if (w > 0.01f) current.core_width_m  = w;
        if (d > 0.01f) current.core_depth_m  = d;
        if (hh> 0.01f) current.core_height_m = hh;
        if (h > 0.005f && h < 10.0f) current.h_target_m = h;

        current.update();
        _syncBuffers();
    }
};