#pragma once
// ============================================================
//  ReactorPanel.hpp
//  Panneau configuration reacteur — touche [R]
//  Position : coin haut-gauche (sous le HUD params)
//  Corrections :
//    - Position hors assemblage (coin gauche, Y=200)
//    - Sync enrichment/moderateur/puissance avec ReactorParams
//    - Flux neutronique GPU affiché (plus "ancienne option" seule)
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cmath>
#include "../physics/NeutronCrossSection.hpp"
#include "SimPanel.hpp"  // pour SimControl + SimState

struct ReactorConfig {
    ReactorType reactorType  = ReactorType::REP;
    float       enrichment   = 0.035f;  // fraction (0..1) — sync depuis ReactorParams
    float       moderateur   = 1.0f;    // ratio densite moderateur — sync depuis ReactorParams
    float       puissance    = 100.0f;  // % puissance nominale — sync depuis ReactorParams
    bool        autoEnrich   = false;   // true = suit la techno selectionnee
    bool showModerator  = true;
    bool showReflector  = true;
    bool showControlRod = true;
    int  nReflectorRings = 1;
    bool changed = false;
    // Mode flux : true=GPU neutronique, false=fallback cosinus
    bool useGPUFlux = true;
};

struct ReactorPanel {
    bool visible = false;

    // PX/PY fixes : coin haut-gauche sous le bloc HUD (y~200)
    static constexpr int PX = 10;
    static constexpr int PY = 200;
    static constexpr int PW = 370;
    static constexpr int PH = 560;

    bool update(ReactorConfig& cfg, float k_eff, SimControl& simCtrl, int sw, int sh) {
        if (IsKeyPressed(KEY_R)) visible = !visible;
        if (!visible) return false;

        // Fond semi-transparent
        DrawRectangle(PX, PY, PW, PH, {10,15,25,245});
        DrawRectangleLines(PX, PY, PW, PH, {100,220,100,255});
        DrawText("CONFIGURATION REACTEUR", PX+10, PY+10, 13, {100,255,100,255});

        Vector2 mouse   = GetMousePosition();
        bool    clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool    changed = false;
        int y = PY + 30;

        // ---- Type de reacteur ----
        DrawText("Type de reacteur :", PX+8, y, 11, {180,180,180,255}); y += 16;
        ReactorType types[] = {
            ReactorType::REP, ReactorType::CANDU,
            ReactorType::RNR_NA, ReactorType::RNR_PB, ReactorType::RHT
        };
        const char* names[] = {"REP","CANDU","RNR-Na","RNR-Pb","RHT"};
        Color tCol[] = {
            {30,80,200,255},{30,140,200,255},
            {200,80,30,255},{180,60,20,255},{80,180,80,255}
        };
        int btnW = (PW - 20) / 5;
        for (int i = 0; i < 5; ++i) {
            Rectangle btn = {(float)(PX+10+i*btnW), (float)y, (float)(btnW-2), 22};
            bool isA = (cfg.reactorType == types[i]);
            bool h   = CheckCollisionPointRec(mouse, btn);
            DrawRectangleRec(btn, isA ? tCol[i] : (h ? Color{60,60,80,255} : Color{30,30,50,255}));
            DrawRectangleLinesEx(btn, 1, isA ? WHITE : LIGHTGRAY);
            int tw = MeasureText(names[i], 10);
            DrawText(names[i], (int)btn.x+(btnW-2-tw)/2, (int)btn.y+5, 10, WHITE);
            if (clicked && h && !isA) {
                cfg.reactorType = types[i];
                if (cfg.autoEnrich)
                    cfg.enrichment = NeutronCrossSection::nominalEnrichment(types[i]);
                changed = true;
            }
        }
        y += 28;

        // ---- Enrichissement ----
        DrawText("Enrichissement :", PX+8, y, 11, {180,180,180,255}); y += 15;
        {
            float eMin = 0.001f, eMax = 0.25f;
            float eR   = fmaxf(0.0f, fminf(1.0f, (cfg.enrichment - eMin)/(eMax - eMin)));
            int bX = PX+8, bW = PW-16, bH = 14;
            DrawRectangle(bX, y, bW, bH, {40,40,60,255});
            DrawRectangle(bX, y, (int)(bW*eR), bH, {60,160,80,255});
            DrawRectangleLines(bX, y, bW, bH, {80,120,80,255});
            Rectangle sr = {(float)bX,(float)y,(float)bW,(float)bH};
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse,sr)) {
                cfg.enrichment = eMin + (eMax-eMin)*fmaxf(0.0f,fminf(1.0f,(mouse.x-bX)/(float)bW));
                cfg.autoEnrich = false; changed = true;
            }
            char eBuf[32];
            snprintf(eBuf, sizeof(eBuf), "%.2f %%", cfg.enrichment*100.0f);
            DrawText(eBuf, PX+8, y+bH+1, 10, LIGHTGRAY);
            // Bouton Auto
            Rectangle btnA = {(float)(PX+PW-105),(float)(y+bH+1), 96, 16};
            bool hA = CheckCollisionPointRec(mouse, btnA);
            DrawRectangleRec(btnA, hA ? Color{50,80,50,255} : Color{30,50,30,255});
            DrawRectangleLinesEx(btnA, 1, LIGHTGRAY);
            DrawText("Auto nominal", (int)btnA.x+4, (int)btnA.y+2, 10, LIGHTGRAY);
            if (clicked && hA) {
                cfg.enrichment = NeutronCrossSection::nominalEnrichment(cfg.reactorType);
                cfg.autoEnrich = true; changed = true;
            }
            y += bH + 20;
        }

        // ---- Moderateur (rho_rel) ----
        DrawText("Moderateur (densite rel.) :", PX+8, y, 11, {180,180,180,255}); y += 15;
        {
            float mMin = 0.0f, mMax = 1.5f;
            float mR   = fmaxf(0.0f, fminf(1.0f, (cfg.moderateur - mMin)/(mMax - mMin)));
            int bX = PX+8, bW = PW-16, bH = 14;
            DrawRectangle(bX, y, bW, bH, {40,40,60,255});
            DrawRectangle(bX, y, (int)(bW*mR), bH, {40,120,200,255});
            DrawRectangleLines(bX, y, bW, bH, {60,100,160,255});
            Rectangle sr = {(float)bX,(float)y,(float)bW,(float)bH};
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse,sr)) {
                cfg.moderateur = mMin + (mMax-mMin)*fmaxf(0.0f,fminf(1.0f,(mouse.x-bX)/(float)bW));
                changed = true;
            }
            char mBuf[32]; snprintf(mBuf, sizeof(mBuf), "%.3f", cfg.moderateur);
            DrawText(mBuf, PX+8, y+bH+1, 10, LIGHTGRAY);
            y += bH + 20;
        }

        // ---- Puissance (%) ----
        DrawText("Puissance (%) :", PX+8, y, 11, {180,180,180,255}); y += 15;
        {
            float pMin = 1.0f, pMax = 120.0f;
            float pR   = fmaxf(0.0f, fminf(1.0f, (cfg.puissance - pMin)/(pMax - pMin)));
            int bX = PX+8, bW = PW-16, bH = 14;
            DrawRectangle(bX, y, bW, bH, {40,40,60,255});
            // couleur rouge si > 100%
            Color pCol = cfg.puissance > 100.0f ? Color{220,80,40,255} : Color{180,140,30,255};
            DrawRectangle(bX, y, (int)(bW*pR), bH, pCol);
            DrawRectangleLines(bX, y, bW, bH, {140,120,60,255});
            Rectangle sr = {(float)bX,(float)y,(float)bW,(float)bH};
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse,sr)) {
                cfg.puissance = pMin + (pMax-pMin)*fmaxf(0.0f,fminf(1.0f,(mouse.x-bX)/(float)bW));
                changed = true;
            }
            char pBuf[32]; snprintf(pBuf, sizeof(pBuf), "%.1f %%", cfg.puissance);
            DrawText(pBuf, PX+8, y+bH+1, 10, LIGHTGRAY);
            y += bH + 20;
        }

        // ---- Mode flux neutronique ----
        DrawText("Mode flux neutronique :", PX+8, y, 11, {180,180,180,255}); y += 15;
        {
            // Bouton GPU
            Rectangle b1 = {(float)(PX+8),  (float)y, 160, 20};
            Rectangle b2 = {(float)(PX+175),(float)y, 160, 20};
            bool h1 = CheckCollisionPointRec(mouse, b1);
            bool h2 = CheckCollisionPointRec(mouse, b2);
            // GPU
            DrawRectangleRec(b1, cfg.useGPUFlux
                ? Color{40,120,200,255}
                : (h1 ? Color{50,50,80,255} : Color{30,30,60,255}));
            DrawRectangleLinesEx(b1, 1, cfg.useGPUFlux ? WHITE : LIGHTGRAY);
            DrawText("GPU Diffusion 2G", (int)b1.x+6, (int)b1.y+4, 10, WHITE);
            // Cosinus
            DrawRectangleRec(b2, !cfg.useGPUFlux
                ? Color{80,80,30,255}
                : (h2 ? Color{50,50,40,255} : Color{30,30,20,255}));
            DrawRectangleLinesEx(b2, 1, !cfg.useGPUFlux ? WHITE : LIGHTGRAY);
            DrawText("Cosinus (fallback)", (int)b2.x+4, (int)b2.y+4, 10, LIGHTGRAY);
            if (clicked && h1 && !cfg.useGPUFlux) { cfg.useGPUFlux = true;  changed = true; }
            if (clicked && h2 &&  cfg.useGPUFlux) { cfg.useGPUFlux = false; changed = true; }
            y += 26;
        }

        // ---- k_eff ----
        DrawRectangle(PX+8, y, PW-16, 40, {20,20,40,200});
        DrawRectangleLines(PX+8, y, PW-16, 40, {60,60,90,255});
        {
            char kBuf[64]; snprintf(kBuf, sizeof(kBuf), "k_eff = %.5f", k_eff);
            Color kCol = (k_eff > 1.005f) ? Color{255,80,80,255}
                       : (k_eff < 0.995f) ? Color{80,160,255,255}
                                           : Color{100,255,100,255};
            DrawText(kBuf, PX+14, y+4, 13, kCol);
            const char* kSt = (k_eff > 1.005f) ? "SURGENERANT"
                            : (k_eff < 0.995f)  ? "SOUS-CRITIQUE" : "CRITIQUE";
            DrawText(kSt, PX+14, y+22, 10, kCol);
        }
        y += 50;

        // ---- Zones 3D ----
        DrawText("Zones 3D :", PX+8, y, 11, {180,180,180,255}); y += 16;
        auto chk = [&](const char* lbl, bool& val, int cx, int cy) {
            Rectangle r = {(float)cx,(float)cy,13,13};
            bool h = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, val ? Color{60,160,80,255} : Color{30,30,50,255});
            DrawRectangleLinesEx(r, 1, LIGHTGRAY);
            if (val) DrawText("X", (int)r.x+2, (int)r.y+1, 11, WHITE);
            DrawText(lbl, cx+17, cy, 11, LIGHTGRAY);
            if (clicked && h) { val = !val; changed = true; }
        };
        chk("Moderateur",  cfg.showModerator,  PX+12, y); y += 18;
        chk("Reflecteur",  cfg.showReflector,  PX+12, y); y += 18;
        chk("Barres ctrl", cfg.showControlRod, PX+12, y); y += 22;

        // ---- Couronnes ----
        DrawText("Couronnes reflecteur :", PX+8, y, 11, {180,180,180,255}); y += 16;
        for (int r = 1; r <= 3; ++r) {
            Rectangle rb = {(float)(PX+8+(r-1)*115),(float)y, 106, 20};
            bool isA = (cfg.nReflectorRings == r);
            bool h   = CheckCollisionPointRec(mouse, rb);
            DrawRectangleRec(rb, isA ? Color{40,100,180,255}
                                     : (h ? Color{50,50,70,255} : Color{30,30,50,255}));
            DrawRectangleLinesEx(rb, 1, isA ? Color{80,160,255,255} : LIGHTGRAY);
            char lbl[16]; snprintf(lbl, sizeof(lbl), "%d anneau%s", r, r>1?"x":"");
            DrawText(lbl, (int)rb.x+8, (int)rb.y+4, 10, WHITE);
            if (clicked && h && !isA) { cfg.nReflectorRings = r; changed = true; }
        }
        y += 28;

        // ---- Boutons Appliquer / Appliquer+Start ----
        // [Appliquer] reinit neutronique seul
        Rectangle btnAp  = {(float)(PX+8),         (float)y, (float)(PW/2-12), 28};
        // [Appliquer + Start] reinit + lance la simulation immediatement
        Rectangle btnAS  = {(float)(PX+PW/2+4),    (float)y, (float)(PW/2-12), 28};

        bool hAp = CheckCollisionPointRec(mouse, btnAp);
        bool hAS = CheckCollisionPointRec(mouse, btnAS);

        DrawRectangleRec(btnAp, hAp ? Color{40,120,40,255} : Color{30,90,30,255});
        DrawRectangleLinesEx(btnAp, 1, LIGHTGRAY);
        DrawText("  Appliquer", (int)btnAp.x+8, (int)btnAp.y+7, 12, WHITE);

        DrawRectangleRec(btnAS, hAS ? Color{30,160,60,255} : Color{20,120,40,255});
        DrawRectangleLinesEx(btnAS, 1, {100,255,100,255});
        DrawText("  Appliquer + Start", (int)btnAS.x+4, (int)btnAS.y+7, 12, {180,255,180,255});

        if (clicked && hAp) {
            cfg.changed = true; changed = true;
        }
        if (clicked && hAS) {
            cfg.changed = true; changed = true;
            simCtrl.state   = SimState::RUNNING;
            simCtrl.simTime = 0.0f;
            visible = false;  // ferme le panneau automatiquement
        }

        // Etat simulation affiché en bas
        y += 34;
        const char* stSt =
            simCtrl.state == SimState::RUNNING ? "EN COURS" :
            simCtrl.state == SimState::PAUSED  ? "EN PAUSE" : "ARRETE";
        Color stCol =
            simCtrl.state == SimState::RUNNING ? Color{100,255,100,255} :
            simCtrl.state == SimState::PAUSED  ? Color{255,200,50,255}  : Color{180,80,80,255};
        // Bouton Stop rapide si en cours
        if (simCtrl.state == SimState::RUNNING) {
            Rectangle btnStop = {(float)(PX+PW/2-45),(float)y,90,22};
            bool hStop = CheckCollisionPointRec(mouse,btnStop);
            DrawRectangleRec(btnStop, hStop ? Color{140,40,40,255} : Color{100,30,30,255});
            DrawRectangleLinesEx(btnStop,1,LIGHTGRAY);
            DrawText("  Pause", (int)btnStop.x+6,(int)btnStop.y+4, 12, WHITE);
            if (clicked && hStop) { simCtrl.state = SimState::PAUSED; changed = true; }
        } else {
            int tw = MeasureText(stSt, 12);
            DrawText(stSt, PX+PW/2-tw/2, y+4, 12, stCol);
        }

        // Fermer si clic hors panneau
        Rectangle panel = {(float)PX,(float)PY,(float)PW,(float)PH+40};
        if (clicked && !CheckCollisionPointRec(mouse, panel)) visible = false;

        return changed;
    }
};