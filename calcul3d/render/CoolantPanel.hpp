#pragma once
// ============================================================
//  CoolantPanel.hpp — Panneau configuration fluide caloporteur
//  [C] : ouvrir/fermer panneau config
//  [F] : cycle mode affichage (géré dans main.cpp)
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "../physics/CoolantModel.hpp"

enum class CoolantDisplayMode { FLECHES, OVERLAY, FLECHES_COULEUR };

struct CoolantPanel {
    bool    visible      = false;
    bool    active       = true;
    CoolantDisplayMode displayMode = CoolantDisplayMode::FLECHES_COULEUR;

    char    bufT[16]  = "286";
    bool    focusT    = false;
    char    bufP[16]  = "155";
    bool    focusP    = false;

    float   arrowAnim = 0.0f;

    static const char* displayModeName(CoolantDisplayMode m) {
        switch(m) {
        case CoolantDisplayMode::FLECHES:        return "Fleches";
        case CoolantDisplayMode::OVERLAY:        return "Overlay T fluide";
        case CoolantDisplayMode::FLECHES_COULEUR:return "Fleches + Couleur";
        }
        return "?";
    }

    // ---- Panneau configuration ----
    bool updatePanel(CoolantParams& params, int sw, int sh) {
        if (IsKeyPressed(KEY_C)) { visible = !visible; focusT = focusP = false; }
        if (!visible) return false;

        const int PX = sw/2 - 200, PY = sh/2 - 230;
        const int PW = 400,        PH = 450;

        DrawRectangle(PX, PY, PW, PH, {10,15,25,245});
        DrawRectangleLines(PX, PY, PW, PH, {80,180,220,255});
        DrawText("FLUIDE CALOPORTEUR", PX+10, PY+10, 14, {80,200,255,255});

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed  = false;
        int  y        = PY + 32;

        // ---- Type de fluide ----
        DrawText("Type de fluide :", PX+10, y, 12, {180,180,180,255});
        y += 18;

        struct FluidOpt { FluidType ft; const char* label; const char* reactor; };
        FluidOpt opts[4] = {
            { FluidType::EAU,      "Eau legere H2O",   "REP"    },
            { FluidType::SODIUM,   "Sodium Na",         "RNR-Na" },
            { FluidType::PLOMB_BI, "Plomb-Bismuth LBE", "RNR-Pb" },
            { FluidType::HELIUM,   "Helium He",         "RHT"    },
        };
        for (int i = 0; i < 4; ++i) {
            bool act = (params.fluid == opts[i].ft);
            DrawCircle(PX+20, y+6, 6, act ? Color{80,200,100,255} : Color{60,60,60,255});
            DrawCircleLines(PX+20, y+6, 6, LIGHTGRAY);
            DrawText(opts[i].label, PX+32, y, 12, LIGHTGRAY);
            char rbuf[32]; snprintf(rbuf,sizeof(rbuf),"(%s)",opts[i].reactor);
            DrawText(rbuf, PX+200, y, 10, {120,120,120,255});
            if (clicked && CheckCollisionPointRec(mouse,{(float)(PX+10),(float)y,320,14})) {
                params.fluid = opts[i].ft;
                switch(opts[i].ft) {
                case FluidType::EAU:
                    params.T_inlet=286.f; params.P_bar=155.f;
                    params.convMode=ConvectionMode::FORCEE; break;
                case FluidType::SODIUM:
                    params.T_inlet=400.f; params.P_bar=1.f;
                    params.convMode=ConvectionMode::COMBINEE; break;
                case FluidType::PLOMB_BI:
                    params.T_inlet=400.f; params.P_bar=1.f;
                    params.convMode=ConvectionMode::FORCEE; break;
                case FluidType::HELIUM:
                    params.T_inlet=250.f; params.P_bar=70.f;
                    params.convMode=ConvectionMode::FORCEE; break;
                }
                snprintf(bufT,sizeof(bufT),"%.0f",params.T_inlet);
                snprintf(bufP,sizeof(bufP),"%.0f",params.P_bar);
                changed = true;
            }
            y += 18;
        }
        y += 6;

        // ---- Mode convection ----
        DrawText("Mode convection :", PX+10, y, 12, {180,180,180,255});
        y += 18;
        struct ConvOpt { ConvectionMode cm; const char* label; const char* tip; };
        ConvOpt covs[3] = {
            {ConvectionMode::FORCEE,    "Forcee (pompe)",     "REP, RNR-Pb, RHT"},
            {ConvectionMode::NATURELLE, "Naturelle (thermo)", "Urgence"          },
            {ConvectionMode::COMBINEE,  "Combinee",           "RNR-Na (defaut)"  },
        };
        for (int i = 0; i < 3; ++i) {
            bool act = (params.convMode == covs[i].cm);
            DrawCircle(PX+20, y+6, 5, act ? Color{80,200,100,255} : Color{60,60,60,255});
            DrawCircleLines(PX+20, y+6, 5, LIGHTGRAY);
            DrawText(covs[i].label, PX+32, y, 12, LIGHTGRAY);
            DrawText(covs[i].tip,   PX+200, y, 10, {100,100,100,255});
            if (clicked && CheckCollisionPointRec(mouse,{(float)(PX+10),(float)y,320,14})) {
                params.convMode = covs[i].cm; changed = true;
            }
            y += 16;
        }
        y += 6;

        // ---- Saisie T et P ----
        auto drawField = [&](const char* label, char* buf, bool& focus,
                              float& val, float vmin, float vmax, int fx, int fy, int fw) {
            DrawText(label, fx, fy-13, 10, {160,160,160,255});
            Rectangle r={(float)fx,(float)fy,(float)fw,22};
            bool hover=CheckCollisionPointRec(mouse,r);
            if (clicked && hover) focus=true;
            if (clicked && !hover && focus) {
                float v=(float)atof(buf);
                val=(v>=vmin&&v<=vmax)?v:val;
                snprintf(buf,16,"%.0f",val);
                focus=false; changed=true;
            }
            DrawRectangleRec(r, focus?Color{25,35,60,255}:Color{15,20,40,255});
            DrawRectangleLinesEx(r,1,focus?Color{80,180,255,255}:Color{60,80,120,255});
            DrawText(buf,fx+4,fy+4,12,WHITE);
            if (focus&&((int)(GetTime()*2)%2==0)) {
                int tw=MeasureText(buf,12);
                DrawRectangle(fx+4+tw,fy+3,2,15,WHITE);
            }
            if (focus) {
                int key=GetCharPressed();
                while(key>0) {
                    int len=strlen(buf);
                    if(key==',')key='.';
                    if((key>='0'&&key<='9')||key=='.'){if(len<15){buf[len]=(char)key;buf[len+1]='\0';}}
                    key=GetCharPressed();
                }
                if(IsKeyPressed(KEY_BACKSPACE)){int len=strlen(buf);if(len>0)buf[len-1]='\0';}
                if(IsKeyPressed(KEY_ENTER)){
                    float v=(float)atof(buf);
                    val=(v>=vmin&&v<=vmax)?v:val;
                    snprintf(buf,16,"%.0f",val);
                    focus=false; changed=true;
                }
            }
        };

        DrawText("Conditions d'entree :", PX+10, y, 12, {180,180,180,255});
        y += 18;
        drawField("T entree (C)", bufT, focusT, params.T_inlet, 0.f, 800.f, PX+15,  y, 120);
        drawField("Pression (bar)", bufP, focusP, params.P_bar, 0.1f,300.f, PX+155, y, 120);
        y += 38;

        // ---- Mode affichage (info seulement, switch via [F] dans main) ----
        DrawRectangle(PX+10, y, PW-20, 22, {20,30,50,200});
        DrawRectangleLines(PX+10, y, PW-20, 22, {60,120,160,255});
        char mbuf[64];
        snprintf(mbuf,sizeof(mbuf),"Affichage [F] : %s",
                 displayModeName(displayMode));
        DrawText(mbuf, PX+16, y+5, 12, {80,200,255,255});
        y += 32;

        // ---- Boutons ----
        // Activer/désactiver
        Rectangle btnToggle={(float)(PX+10),(float)y,130,26};
        bool hT=CheckCollisionPointRec(mouse,btnToggle);
        DrawRectangleRec(btnToggle, active?(hT?Color{40,100,40,255}:Color{30,80,30,255})
                                         :(hT?Color{100,40,40,255}:Color{80,30,30,255}));
        DrawRectangleLinesEx(btnToggle,1,LIGHTGRAY);
        DrawText(active?"  ACTIF":"  INACTIF",PX+16,y+6,12,WHITE);
        if(clicked&&hT){active=!active;changed=true;}

        // Appliquer
        Rectangle btnApply={(float)(PX+150),(float)y,130,26};
        bool hA=CheckCollisionPointRec(mouse,btnApply);
        DrawRectangleRec(btnApply,hA?Color{40,80,140,255}:Color{30,60,120,255});
        DrawRectangleLinesEx(btnApply,1,LIGHTGRAY);
        DrawText("  Appliquer",PX+156,y+6,12,WHITE);
        if(clicked&&hA) changed=true;

        Rectangle pr={(float)PX,(float)PY,(float)PW,(float)PH};
        if(clicked&&!CheckCollisionPointRec(mouse,pr)) visible=false;

        return changed;
    }

    // ================================================================
    //  RENDU 3D — flèches dans les interstices entre assemblages
    // ================================================================
    void draw3DArrows(const GridData& grid, const CoolantModel& model,
                      const RenderOptions& ropt) {
        if (!active) return;
        if (displayMode == CoolantDisplayMode::OVERLAY) return;

        arrowAnim += GetFrameTime() * 1.2f;
        if (arrowAnim > 1.0f) arrowAnim -= 1.0f;

        float step   = grid.dims.width + grid.dims.spacing;
        float cubeH  = ropt.cubeHeight;
        float gap    = grid.dims.spacing;           // largeur interstice
        float halfW  = grid.dims.width * 0.5f;

        // Vitesse max pour normalisation
        float v_max = 0.001f;
        for (int r=0; r<grid.rows; ++r)
            for (int c=0; c<grid.cols; ++c)
                v_max = fmaxf(v_max, model.getVfluid(r,c));

        // ---- Interstices VERTICAUX (entre col c et c+1, pour chaque row) ----
        for (int r = 0; r < grid.rows; ++r) {
            for (int c = 0; c < grid.cols-1; ++c) {
                // Vérifie qu'au moins un des deux côtés a un assemblage
                bool hasL = false, hasR = false;
                float posX_L = 0, posX_R = 0, posZ = 0;
                for (const auto& cube : grid.cubes) {
                    if (cube.row==r && cube.col_idx==c)   { hasL=true; posX_L=cube.pos.x; posZ=cube.pos.z; }
                    if (cube.row==r && cube.col_idx==c+1) { hasR=true; posX_R=cube.pos.x; }
                }
                if (!hasL && !hasR) continue;

                float v1 = model.getVfluid(r, c);
                float v2 = model.getVfluid(r, c+1);
                float v  = (v1 + v2) * 0.5f;
                float v_norm = fminf(v / v_max, 1.0f);

                // Position = milieu de l'interstice
                float x_inter = hasL ? (posX_L + halfW + gap*0.5f)
                                     : (posX_R - halfW - gap*0.5f);

                // Couleur : bleu froid → cyan chaud selon vitesse
                Color col = {
                    (unsigned char)(10),
                    (unsigned char)(80 + (int)(170*v_norm)),
                    (unsigned char)(180 + (int)(75*v_norm)),
                    220
                };

                // Épaisseur visuelle ∝ vitesse (plusieurs lignes parallèles)
                int nLines = 1 + (int)(v_norm * 3);  // 1 à 4 lignes
                for (int nl = 0; nl < nLines; ++nl) {
                    float xOff = (nl - nLines*0.5f) * gap * 0.15f;

                    // 4 segments animés qui montent
                    for (int seg = 0; seg < 4; ++seg) {
                        float phase = fmodf(arrowAnim + seg * 0.25f, 1.0f);
                        float yBot  = -cubeH*0.5f + phase * cubeH * 1.1f;
                        float yTop  = yBot + cubeH * 0.18f;

                        Vector3 bot = {x_inter+xOff, yBot, posZ};
                        Vector3 top = {x_inter+xOff, yTop, posZ};
                        DrawLine3D(bot, top, col);

                        // Pointe de flèche (seulement sur la ligne centrale)
                        if (nl == nLines/2) {
                            float s = gap * 0.3f * (0.5f + v_norm*0.5f);
                            DrawLine3D(top, {x_inter+xOff-s, yTop-s*0.8f, posZ}, col);
                            DrawLine3D(top, {x_inter+xOff+s, yTop-s*0.8f, posZ}, col);
                        }
                    }
                }
            }
        }

        // ---- Interstices HORIZONTAUX (entre row r et r+1, pour chaque col) ----
        for (int r = 0; r < grid.rows-1; ++r) {
            for (int c = 0; c < grid.cols; ++c) {
                bool hasT = false, hasB = false;
                float posZ_T = 0, posZ_B = 0, posX = 0;
                for (const auto& cube : grid.cubes) {
                    if (cube.row==r   && cube.col_idx==c) { hasT=true; posZ_T=cube.pos.z; posX=cube.pos.x; }
                    if (cube.row==r+1 && cube.col_idx==c) { hasB=true; posZ_B=cube.pos.z; }
                }
                if (!hasT && !hasB) continue;

                float v1 = model.getVfluid(r,   c);
                float v2 = model.getVfluid(r+1, c);
                float v  = (v1+v2)*0.5f;
                float v_norm = fminf(v/v_max, 1.0f);

                float z_inter = hasT ? (posZ_T + halfW + gap*0.5f)
                                     : (posZ_B - halfW - gap*0.5f);

                Color col = {
                    (unsigned char)(10),
                    (unsigned char)(80+(int)(170*v_norm)),
                    (unsigned char)(180+(int)(75*v_norm)),
                    180
                };

                int nLines = 1 + (int)(v_norm * 2);
                for (int nl = 0; nl < nLines; ++nl) {
                    float zOff = (nl - nLines*0.5f) * gap * 0.15f;
                    for (int seg = 0; seg < 4; ++seg) {
                        float phase = fmodf(arrowAnim + seg*0.25f, 1.0f);
                        float yBot  = -cubeH*0.5f + phase*cubeH*1.1f;
                        float yTop  = yBot + cubeH*0.18f;
                        DrawLine3D({posX, yBot, z_inter+zOff},
                                   {posX, yTop, z_inter+zOff}, col);
                        if (nl == nLines/2) {
                            float s = gap*0.3f*(0.5f+v_norm*0.5f);
                            DrawLine3D({posX,yTop,z_inter+zOff},
                                       {posX,yTop-s*0.8f,z_inter+zOff-s},col);
                            DrawLine3D({posX,yTop,z_inter+zOff},
                                       {posX,yTop-s*0.8f,z_inter+zOff+s},col);
                        }
                    }
                }
            }
        }
    }

    // ================================================================
    //  OVERLAY 2D température fluide
    // ================================================================
    void draw2DOverlay(const GridData& grid, const CoolantModel& model,
                       int sw, int sh) {
        if (!active) return;
        if (displayMode == CoolantDisplayMode::FLECHES) return;

        int cellPx = 28;
        int panelW = grid.cols * cellPx + 2;
        int panelH = grid.rows * cellPx + 30;
        int panelX = 10;
        int panelY = sh - panelH - 10;

        DrawRectangle(panelX, panelY, panelW, panelH, {5,10,20,220});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {80,180,220,255});
        DrawText("FLUIDE T(C) / v(m/s)", panelX+4, panelY+4, 10, {80,200,255,255});

        float Tf_min = model.params.T_inlet, Tf_max = model.params.T_inlet+1.f;
        for (int r=0; r<grid.rows; ++r)
            for (int c=0; c<grid.cols; ++c) {
                float t=model.getTfluid(r,c);
                Tf_min=fminf(Tf_min,t); Tf_max=fmaxf(Tf_max,t);
            }

        int mapY = panelY+20;
        for (int r=0; r<grid.rows; ++r) {
            for (int c=0; c<grid.cols; ++c) {
                int px=panelX+c*cellPx, py=mapY+r*cellPx;
                float T_f=model.getTfluid(r,c);
                float norm=(Tf_max>Tf_min)?(T_f-Tf_min)/(Tf_max-Tf_min):0.f;
                Color col={(unsigned char)(norm*80),
                           (unsigned char)(100+norm*155),
                           (unsigned char)(200+norm*55),200};
                DrawRectangle(px,py,cellPx,cellPx,col);
                DrawRectangleLines(px,py,cellPx,cellPx,{0,0,0,60});
                char vbuf[8];
                snprintf(vbuf,sizeof(vbuf),"%.2f",model.getVfluid(r,c));
                DrawText(vbuf,px+2,py+cellPx/2-4,8,WHITE);
            }
        }
        char buf[32];
        snprintf(buf,sizeof(buf),"%.0f",Tf_min); DrawText(buf,panelX+2,panelY+panelH-10,9,LIGHTGRAY);
        snprintf(buf,sizeof(buf),"%.0fC",Tf_max); DrawText(buf,panelX+panelW-38,panelY+panelH-10,9,LIGHTGRAY);
    }

    // ---- Info overlay haut-droite ----
    void drawInfoOverlay(const CoolantModel& model, int sw) {
        if (!active) return;
        int x=sw-245, y=10;
        DrawRectangle(x,y,235,95,{0,0,0,170});
        DrawRectangleLines(x,y,235,95,{80,180,220,255});
        DrawText("CALOPORTEUR",x+8,y+8,13,{80,200,255,255});
        char buf[64];
        snprintf(buf,sizeof(buf),"%s",CoolantModel::fluidName(model.params.fluid));
        DrawText(buf,x+8,y+26,11,LIGHTGRAY);
        snprintf(buf,sizeof(buf),"T_in=%.0fC  P=%.0fbar",
                 model.params.T_inlet,model.params.P_bar);
        DrawText(buf,x+8,y+42,11,LIGHTGRAY);
        const char* mstr=model.params.convMode==ConvectionMode::FORCEE?"Forcee":
                         model.params.convMode==ConvectionMode::NATURELLE?"Naturelle":"Combinee";
        DrawText(mstr,x+8,y+58,11,{120,200,120,255});
        // Mode affichage courant
        snprintf(buf,sizeof(buf),"[F] %s",displayModeName(displayMode));
        DrawText(buf,x+8,y+74,11,{80,180,255,255});
    }
};