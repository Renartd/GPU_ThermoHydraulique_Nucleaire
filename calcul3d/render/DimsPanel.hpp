#pragma once
// ============================================================
//  DimsPanel.hpp  v5  —  Touche P : dimensions physiques
//
//  Trois grandeurs indépendantes :
//    assy_width_m  : largeur NETTE de l'assemblage (boîtier)
//    assy_gap_m    : jeu inter-assemblage (espace vide)
//    assy_height_m : hauteur active
//
//  Pitch = width + gap  (calculé, affiché en lecture seule)
//  Contrainte anti-collision : width < pitch  toujours vérifiée
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <functional>
#include "../core/GridData.hpp"

struct DimsPanel {
    bool visible = false;
    char bufWidth [16] = "0.2070";
    char bufGap   [16] = "0.0073";
    char bufHeight[16] = "3.658";
    int  activeField = -1;  // 0=width  1=gap  2=height

    std::function<void(const MeshConfig&)> onChange;
    MeshConfig current;

    void init(const MeshConfig& mc) { current = mc; _sync(); }

    void _sync() {
        snprintf(bufWidth,  sizeof(bufWidth),  "%.4f", current.assy_width_m);
        snprintf(bufGap,    sizeof(bufGap),    "%.4f", current.assy_gap_m);
        snprintf(bufHeight, sizeof(bufHeight), "%.3f", current.assy_height_m);
    }

    bool update(int sw, int sh) {
        if (IsKeyPressed(KEY_P)) { visible = !visible; activeField = -1; }
        if (!visible) return false;

        const int PW=400, PH=340;
        const int PX=sw/2-PW/2, PY=sh/2-PH/2;
        DrawRectangle(PX, PY, PW, PH, {8,10,30,248});
        DrawRectangleLines(PX, PY, PW, PH, {80,140,255,255});
        DrawText("DIMENSIONS ASSEMBLAGE  [P]", PX+12, PY+10, 13,
                 {100,180,255,255});
        DrawLine(PX, PY+28, PX+PW, PY+28, {40,60,120,200});

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed  = false;
        int y = PY + 38;

        // ── Schéma anti-collision ─────────────────────────────
        {
            int sx=PX+12, sy=y;
            float scale=180.0f / current.assy_pitch_m;
            int pw=(int)(current.assy_pitch_m * scale);  // pitch en px
            int ww=(int)(current.assy_width_m * scale);  // width en px
            int gw=pw-ww;                                 // gap en px

            // Deux assemblages côte à côte
            DrawRectangle(sx,      sy, ww, 28, {40,80,140,200});
            DrawRectangle(sx+ww,   sy, gw, 28, {15,15,40,180});   // gap
            DrawRectangle(sx+pw,   sy, ww, 28, {40,80,140,200});

            DrawRectangleLines(sx,    sy, ww, 28, {80,140,255,255});
            DrawRectangleLines(sx+pw, sy, ww, 28, {80,140,255,255});

            // Labels
            char lb[20];
            snprintf(lb,sizeof(lb),"%.4fm",current.assy_width_m);
            DrawText(lb, sx+2, sy+8, 9, LIGHTGRAY);
            if (gw > 14) {
                snprintf(lb,sizeof(lb),"%.3f",current.assy_gap_m);
                DrawText(lb, sx+ww+1, sy+8, 8, {180,180,255,200});
            }
            // Flèche pitch
            DrawLine(sx, sy+32, sx+pw, sy+32, {100,160,255,200});
            DrawLine(sx, sy+29, sx, sy+35, {100,160,255,200});
            DrawLine(sx+pw, sy+29, sx+pw, sy+35, {100,160,255,200});
            snprintf(lb,sizeof(lb),"pitch=%.4fm",current.assy_pitch_m);
            DrawText(lb, sx+pw/2-30, sy+34, 9, {100,160,255,200});
            y += 48;
        }

        DrawLine(PX+8,y,PX+PW-8,y,{30,50,100,160}); y+=8;

        // ── Champs de saisie ──────────────────────────────────
        changed |= _row("Largeur nette (m) :", bufWidth,  0, PX, y); y+=28;
        changed |= _row("Jeu inter-assy (m) :", bufGap,    1, PX, y); y+=28;
        changed |= _row("Hauteur active (m) :", bufHeight, 2, PX, y); y+=28;

        // Pitch en lecture seule
        char pitchStr[32];
        snprintf(pitchStr,sizeof(pitchStr),"%.4f m  (calculé)",
                 current.assy_pitch_m);
        DrawText("Pitch centre/centre :", PX+12, y+5, 11, {140,160,200,200});
        DrawRectangle(PX+210, y, 150, 22, {15,20,45,200});
        DrawRectangleLinesEx({(float)(PX+210),(float)y,150,22},1,{40,60,120,180});
        DrawText(pitchStr, PX+215, y+5, 11, {120,180,255,255});
        y+=32;

        DrawLine(PX+8,y,PX+PW-8,y,{30,50,100,160}); y+=10;

        // ── Presets ───────────────────────────────────────────
        DrawText("Standards :", PX+12, y, 11, {120,150,200,200}); y+=16;

        struct Preset {
            const char* label;
            float width, gap, height;
        };
        // Sources : IAEA-TECDOC, RCC-C, conception réacteurs
        Preset presets[] = {
            {"REP 900MW",  0.2070f, 0.0073f, 3.658f},
            {"REP 1300MW", 0.2136f, 0.0014f, 4.267f},
            {"REP 1450MW", 0.2136f, 0.0014f, 4.480f},
            {"CANDU",      0.2800f, 0.0060f, 5.940f},
            {"RNR-Na",     0.1660f, 0.0060f, 1.000f},
        };
        int bw=76, bh=22, gap2=4;
        for (int i=0;i<5;i++) {
            Rectangle br={(float)(PX+12+i*(bw+gap2)),(float)y,
                          (float)bw,(float)bh};
            bool hov=CheckCollisionPointRec(mouse,br);
            DrawRectangleRec(br,hov?Color{30,60,130,255}:Color{18,40,90,200});
            DrawRectangleLinesEx(br,1,{55,100,200,200});
            int tw=MeasureText(presets[i].label,9);
            DrawText(presets[i].label,
                     (int)br.x+((int)bw-tw)/2,(int)br.y+6,9,LIGHTGRAY);
            if (clicked&&hov) {
                snprintf(bufWidth, sizeof(bufWidth),  "%.4f",presets[i].width);
                snprintf(bufGap,   sizeof(bufGap),    "%.4f",presets[i].gap);
                snprintf(bufHeight,sizeof(bufHeight), "%.3f",presets[i].height);
                changed=true;
            }
        }
        y+=30;

        if (changed) { _apply(); if (onChange) onChange(current); }

        // Infos cœur
        {
            char buf[80];
            snprintf(buf,sizeof(buf),
                "Coeur : %.3fm × %.3fm × %.3fm  |  %d assy",
                current.core_width_m(), current.core_depth_m(),
                current.core_height_m(),
                current.n_assy_cols * current.n_assy_rows);
            DrawText(buf, PX+12, y, 10, {100,140,200,200}); y+=14;
        }

        // Fermer
        Rectangle cr={(float)(PX+PW-62),(float)(PY+PH-34),56,26};
        bool ov=CheckCollisionPointRec(mouse,cr);
        DrawRectangleRec(cr,ov?Color{100,40,40,255}:Color{65,22,22,200});
        DrawRectangleLinesEx(cr,1,{180,70,70,255});
        DrawText("Fermer",PX+PW-56,PY+PH-28,11,LIGHTGRAY);
        if (clicked&&ov) visible=false;

        return changed;
    }

private:
    bool _row(const char* label, char* buf, int id, int PX, int y) {
        DrawText(label, PX+12, y+5, 11, {160,190,220,255});
        Rectangle r={(float)(PX+210),(float)y,110,22};
        bool isA=(activeField==id);
        Vector2 mouse=GetMousePosition();
        bool clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        DrawRectangleRec(r,isA?Color{15,35,80,255}:Color{10,22,55,200});
        DrawRectangleLinesEx(r,1,isA?Color{80,160,255,255}:Color{40,80,180,200});
        DrawText(buf,(int)r.x+5,(int)r.y+5,11,LIGHTGRAY);
        if (clicked) {
            if (CheckCollisionPointRec(mouse,r)) activeField=id;
            else if (activeField==id) activeField=-1;
        }
        if (isA) {
            int key=GetCharPressed(),len=(int)strlen(buf);
            while (key>0) {
                if (len<12&&(key>='0'&&key<='9'||key=='.'))
                    {buf[len]=(char)key;buf[len+1]='\0';}
                key=GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)&&len>0) buf[len-1]='\0';
            if (IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_TAB)){
                activeField=-1; return true;
            }
        }
        return false;
    }

    void _apply() {
        float w=(float)atof(bufWidth);
        float g=(float)atof(bufGap);
        float h=(float)atof(bufHeight);
        if (w>0.01f&&w<2.0f)  current.assy_width_m  = w;
        if (g>0.0f  &&g<0.5f) current.assy_gap_m    = g;
        if (h>0.1f  &&h<30.f) current.assy_height_m = h;
        current.update();  // recalcule pitch + anti-collision
        _sync();
    }
};