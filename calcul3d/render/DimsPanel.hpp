#pragma once
// ============================================================
//  DimsPanel.hpp  v3.0
//  Touche D : ouvrir/fermer
//
//  Subdivision intra-assemblage :
//    sub_xy : N divisions en X et Z par assemblage (1..65536)
//    sub_z  : M divisions axiales (1..1024)
//
//  Chaque assemblage → sub_xy × sub_xy × sub_z micro-cubes
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <functional>
#include <algorithm>
#include "../core/GridData.hpp"

struct DimsPanel {
    bool visible = false;
    char bufSubXY[16] = "8";
    char bufSubZ [16] = "149";  // 8 × (4.0/0.214) ≈ 149 → cubes réguliers
    int  activeField = -1;
    std::function<void(const MeshConfig&)> onChange;
    MeshConfig current;

    void init(const MeshConfig& mc) { current=mc; _sync(); }

    void _sync() {
        snprintf(bufSubXY,sizeof(bufSubXY),"%d",current.sub_xy);
        snprintf(bufSubZ, sizeof(bufSubZ), "%d",current.sub_z);
        current.update();
    }

    bool update(int sw, int sh) {
        if (IsKeyPressed(KEY_D)) { visible=!visible; activeField=-1; }
        if (!visible) return false;

        const int PW=440, PH=480;
        const int PX=sw/2-PW/2, PY=sh/2-PH/2;
        DrawRectangle(PX,PY,PW,PH,{8,10,22,248});
        DrawRectangleLines(PX,PY,PW,PH,{100,200,100,255});
        DrawText("MAILLAGE INTRA-ASSEMBLAGE",PX+12,PY+10,14,{100,255,100,255});
        DrawLine(PX,PY+28,PX+PW,PY+28,{40,80,40,200});

        Vector2 mouse=GetMousePosition();
        bool clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool changed=false;
        int y=PY+36;

        // Schéma
        {
            int sx=PX+12,sy=y;
            int aw=88,ah=66;
            int N=std::min(current.sub_xy,8);
            float cw=(float)aw/N,ch=(float)ah/N;
            for(int i=0;i<N;i++) for(int j=0;j<N;j++){
                Color cc=((i+j)%2==0)?Color{40,90,40,200}:Color{25,60,35,200};
                DrawRectangle(sx+(int)(i*cw),sy+(int)(j*ch),(int)cw-1,(int)ch-1,cc);
            }
            DrawRectangleLines(sx,sy,aw,ah,{100,220,100,255});
            char nb[32];
            snprintf(nb,sizeof(nb),"%dx%d / assy",current.sub_xy,current.sub_xy);
            DrawText(nb,sx,sy+ah+4,9,{120,200,120,255});

            int bx=sx+aw+16,bw=22;
            int Mz=std::min(current.sub_z,16);
            float ch2=(float)ah/Mz;
            for(int k=0;k<Mz;k++){
                Color cc2=(k%2==0)?Color{25,50,90,200}:Color{20,40,75,200};
                DrawRectangle(bx,sy+(int)(k*ch2),bw-1,(int)ch2-1,cc2);
            }
            DrawRectangleLines(bx,sy,bw,ah,{80,160,255,255});
            snprintf(nb,sizeof(nb),"%d axial",current.sub_z);
            DrawText(nb,bx,sy+ah+4,9,{100,180,255,200});

            DrawText("(chaque carre = 1 cellule elementaire)",
                     PX+12,sy+ah+18,9,{80,120,80,180});
            y+=ah+32;
        }

        DrawLine(PX+8,y,PX+PW-8,y,{30,60,30,160}); y+=8;

        // sub_xy
        DrawText("Divisions XZ par assemblage :",PX+12,y,12,{180,220,180,255}); y+=17;
        DrawText("1=1cell  4=16cells  16=256cells  256=65536cells",
                 PX+12,y,10,{100,140,100,200}); y+=15;
        changed|=_intField("sub_xy :",bufSubXY,0,PX,y);
        y+=28;

        // Presets XZ
        {
            int vals[]={1,2,4,8,16,32,64,256,1024,4096};
            int bx0=PX+12,bw=38,bh=20,gap=3;
            for(int i=0;i<10;i++){
                Rectangle br={(float)(bx0+i*(bw+gap)),(float)y,(float)bw,(float)bh};
                bool hov=CheckCollisionPointRec(mouse,br);
                bool cur=(current.sub_xy==vals[i]);
                DrawRectangleRec(br,cur?Color{40,110,40,255}:(hov?Color{30,75,30,255}:Color{18,48,18,200}));
                DrawRectangleLinesEx(br,1,cur?Color{100,255,100,255}:Color{50,110,50,200});
                char lb[8]; snprintf(lb,sizeof(lb),"%d",vals[i]);
                int tw=MeasureText(lb,9);
                DrawText(lb,(int)br.x+((int)bw-tw)/2,(int)br.y+5,9,LIGHTGRAY);
                if(clicked&&hov){snprintf(bufSubXY,sizeof(bufSubXY),"%d",vals[i]);changed=true;}
            }
            y+=26;
        }

        DrawLine(PX+8,y,PX+PW-8,y,{30,50,80,160}); y+=8;

        // sub_z
        DrawText("Divisions axiales Y :",PX+12,y,12,{180,200,255,255}); y+=17;
        changed|=_intField("sub_z  :",bufSubZ,1,PX,y);
        y+=28;
        {
            int vals[]={4,8,16,32,64,128,256,512};
            int bx0=PX+12,bw=38,bh=20,gap=3;
            for(int i=0;i<8;i++){
                Rectangle br={(float)(bx0+i*(bw+gap)),(float)y,(float)bw,(float)bh};
                bool hov=CheckCollisionPointRec(mouse,br);
                bool cur=(current.sub_z==vals[i]);
                DrawRectangleRec(br,cur?Color{25,55,120,255}:(hov?Color{20,45,95,255}:Color{12,30,70,200}));
                DrawRectangleLinesEx(br,1,cur?Color{80,160,255,255}:Color{35,75,170,200});
                char lb[8]; snprintf(lb,sizeof(lb),"%d",vals[i]);
                int tw=MeasureText(lb,9);
                DrawText(lb,(int)br.x+((int)bw-tw)/2,(int)br.y+5,9,LIGHTGRAY);
                if(clicked&&hov){snprintf(bufSubZ,sizeof(bufSubZ),"%d",vals[i]);changed=true;}
            }
            y+=26;
        }

        if(changed){ _applyBuffers(); if(onChange) onChange(current); }

        // Résultats
        DrawLine(PX+8,y,PX+PW-8,y,{30,60,30,160}); y+=8;
        char buf[128];

        snprintf(buf,sizeof(buf),"Cellule : %.5fm x %.5fm x %.4fm",
                 current.dx,current.dz,current.dy);
        DrawText(buf,PX+12,y,11,{160,200,160,255}); y+=15;

        long long tot=current.total3d();
        Color tc=tot<50000LL?Color{100,220,100,255}:
                 tot<1000000LL?Color{255,200,60,255}:Color{255,100,60,255};
        if(tot<1000000LL)
            snprintf(buf,sizeof(buf),"Total 3D : %dx%dx%d = %lld cellules",
                     current.cols,current.rows,current.slices,tot);
        else
            snprintf(buf,sizeof(buf),"Total 3D : %dx%dx%d = %.2fM cellules",
                     current.cols,current.rows,current.slices,(float)tot/1e6f);
        DrawText(buf,PX+12,y,11,tc); y+=15;

        float mem=current.estimatedMemMB();
        Color mc2=mem<512?Color{100,220,100,255}:
                  mem<4096?Color{255,200,60,255}:Color{255,80,60,255};
        snprintf(buf,sizeof(buf),"Memoire GPU estimee : %.0f Mo  (%.1f Go)",mem,mem/1024.f);
        DrawText(buf,PX+12,y,11,mc2); y+=15;

        Color arc=current.aspectOK()?Color{100,220,100,255}:Color{255,120,60,255};
        snprintf(buf,sizeof(buf),"Rapport d aspect : %.2f  %s",
                 current.aspect_ratio,current.aspectOK()?"OK":"DEGRADE!");
        DrawText(buf,PX+12,y,11,arc); y+=15;

        if(current.gpuRequired()){
            DrawRectangle(PX+8,y,PW-16,18,{60,20,0,200});
            DrawText("GPU REQUIS pour ce maillage",PX+14,y+3,11,{255,160,60,255});
            y+=22;
        }

        // Fermer
        Rectangle cr={(float)(PX+PW-62),(float)(PY+PH-34),56,26};
        bool ov=CheckCollisionPointRec(mouse,cr);
        DrawRectangleRec(cr,ov?Color{120,40,40,255}:Color{70,25,25,200});
        DrawRectangleLinesEx(cr,1,{200,80,80,255});
        DrawText("Fermer",PX+PW-56,PY+PH-28,11,LIGHTGRAY);
        if(clicked&&ov) visible=false;

        return changed;
    }

private:
    bool _intField(const char* label, char* buf, int id, int PX, int y){
        DrawText(label,PX+12,y+5,11,{160,190,160,255});
        Rectangle r={(float)(PX+210),(float)y,110,22};
        bool isA=(activeField==id);
        Vector2 mouse=GetMousePosition();
        bool clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        DrawRectangleRec(r,isA?Color{20,50,20,255}:Color{12,32,16,200});
        DrawRectangleLinesEx(r,1,isA?Color{80,200,80,255}:Color{45,110,50,200});
        DrawText(buf,(int)r.x+5,(int)r.y+5,11,LIGHTGRAY);
        if(clicked){
            if(CheckCollisionPointRec(mouse,r)) activeField=id;
            else if(activeField==id) activeField=-1;
        }
        if(isA){
            int key=GetCharPressed(),len=(int)strlen(buf);
            while(key>0){
                if(len<10&&key>='0'&&key<='9'){buf[len]=(char)key;buf[len+1]='\0';}
                key=GetCharPressed();
            }
            if(IsKeyPressed(KEY_BACKSPACE)&&len>0) buf[len-1]='\0';
            if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_TAB)){
                activeField=-1; return true;
            }
        }
        return false;
    }

    void _applyBuffers(){
        int sxy=atoi(bufSubXY), sz=atoi(bufSubZ);
        if(sxy>=1&&sxy<=65536) current.sub_xy=sxy;
        if(sz >=1&&sz <=1024)  current.sub_z =sz;
        current.update();
        _sync();
    }
};