#pragma once
// ============================================================
//  XenonPanel.hpp  v1.0
//  Panneau de visualisation Xénon-135 / Iode-135
//  Touche X : ouvrir/fermer
//
//  AFFICHE :
//    - Concentrations moyennes Xe / I / Sm (u.a.)
//    - Empoisonnement Xe total (pcm)
//    - Courbes temporelles Xe(t) et I(t) post-arrêt simulées
//    - Carte 2D de l'empoisonnement ΔΣA2 par assemblage
//    - Mode bore (REP) : saisie concentration bore (ppm)
//    - Indicateur "pic xénon" post-arrêt
//
//  PHYSIQUE AFFICHÉE :
//    A pleine puissance REP 900MW :
//      Xe_eq ≈ quelques unités (normalisé φ₀)
//      ρ_Xe  ≈ −2000 à −3000 pcm
//    Post-arrêt (φ=0) :
//      Xe monte pendant ~8h (I → Xe, Xe ne brûle plus)
//      Pic xe ≈ −30 000 pcm (réacteur ne peut pas redémarrer)
//      Xe disparaît en ~20h (décroissance radioactive)
// ============================================================
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include "../physics/XenonModel.hpp"

struct XenonPanel {
    bool visible = false;

    // Bore (REP uniquement)
    char boreBuf[16] = "0";
    bool boreActive  = false;
    float C_bore_ppm = 0.0f;

    // Historique pour les courbes (xe(t), iode(t))
    struct XePoint {
        float t;       // temps simulé (s)
        float xe_avg;  // Xe moyen (u.a.)
        float i_avg;   // I moyen (u.a.)
        float rho_pcm; // empoisonnement (pcm)
        float power;   // puissance relative
    };
    std::vector<XePoint> history;
    float _lastRecord = -1.0f;
    static constexpr float RECORD_INTERVAL = 60.0f; // 1 min sim entre points
    static constexpr int   MAX_POINTS      = 500;

    void recordPoint(float t, const XenonModel& xm, float power_rel) {
        if (t - _lastRecord < RECORD_INTERVAL && _lastRecord >= 0.0f) return;
        _lastRecord = t;
        float i_avg = 0.0f;
        int N = (int)xm.iodine.size();
        for (int i=0;i<N;++i) i_avg += xm.iodine[i];
        if (N > 0) i_avg /= N;
        history.push_back({t, xm.xe_avg, i_avg,
                           xm.reactivity_xe_pcm, power_rel});
        if ((int)history.size() > MAX_POINTS)
            history.erase(history.begin());
    }

    void reset() {
        history.clear();
        _lastRecord = -1.0f;
    }

    // ── update() — appeler entre BeginDrawing/EndDrawing ────
    // Retourne true si C_bore_ppm a changé
    bool update(const XenonModel& xm, float simTime,
                float power_rel, bool isREP,
                int sw, int sh)
    {
        if (IsKeyPressed(KEY_X)) { visible = !visible; }
        if (!visible) return false;

        const int PW = 460, PH = 540;
        const int PX = sw/2 - PW/2, PY = sh/2 - PH/2;

        DrawRectangle(PX, PY, PW, PH, {8,12,24,248});
        DrawRectangleLines(PX, PY, PW, PH, {120,80,200,255});
        DrawText("XENON-135 / IODE-135 / SAMARIUM-149",
                 PX+10, PY+8, 13, {200,150,255,255});
        DrawLine(PX, PY+26, PX+PW, PY+26, {60,40,100,200});

        Vector2 mouse = GetMousePosition();
        bool clicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        int y = PY + 32;

        // ── Concentrations moyennes ─────────────────────────
        char buf[128];
        _sectionTitle("Concentrations moyennes", PX+10, y);
        y += 18;

        float xe_n = xm.xe_avg;
        float i_n  = 0.0f;
        float sm_n = 0.0f;
        int N = (int)xm.xenon.size();
        for (int i=0;i<N;++i) {
            i_n  += xm.iodine[i];
            sm_n += xm.samarium[i];
        }
        if (N > 0) { i_n /= N; sm_n /= N; }

        // Barres colorées proportionnelles au max théorique
        float xe_max_ref = xe_n * 2.0f + 1e-10f; // ← s'adapte au pic
        _bar("Xe-135", xe_n, xe_max_ref, {180,80,255,255},  PX+10, y, PW-20, 18);
        y += 22;
        _bar("I-135",  i_n,  i_n * 1.5f + 1e-10f, {80,180,255,255}, PX+10, y, PW-20, 18);
        y += 22;
        _bar("Sm-149", sm_n, sm_n * 1.5f + 1e-10f, {80,255,160,255}, PX+10, y, PW-20, 18);
        y += 26;

        // ── Empoisonnement ──────────────────────────────────
        _sectionTitle("Empoisonnement neutronique", PX+10, y);
        y += 18;

        float rho_xe = xm.reactivity_xe_pcm;
        Color rhoCol = (rho_xe < -5000) ? Color{255,80,80,255}
                     : (rho_xe < -1000) ? Color{255,180,60,255}
                                        : Color{100,220,100,255};
        snprintf(buf, sizeof(buf), "rho_Xe  = %+.0f pcm", rho_xe);
        DrawText(buf, PX+12, y, 13, rhoCol);
        y += 18;

        // Indicateur pic xénon (post-arrêt)
        bool peakRisk = (rho_xe < -8000.0f);
        if (peakRisk) {
            DrawRectangle(PX+10, y, PW-20, 18, {80,20,20,200});
            DrawText("  PIC XENON : redemar rage impossible",
                     PX+12, y+3, 11, {255,120,120,255});
        } else if (rho_xe < -3000.0f) {
            DrawRectangle(PX+10, y, PW-20, 18, {60,40,0,180});
            DrawText("  Xenon eleve — surveiller puissance",
                     PX+12, y+3, 11, {255,220,100,255});
        } else {
            DrawText("  Empoisonnement nominal", PX+12, y+3, 11, {100,200,100,255});
        }
        y += 24;

        // ── Bore (REP uniquement) ───────────────────────────
        bool boreChanged = false;
        if (isREP) {
            _sectionTitle("Controle chimique — Bore", PX+10, y);
            y += 18;
            DrawText("C_bore (ppm) :", PX+12, y+4, 11, {160,160,200,255});
            Rectangle bf = {(float)(PX+130), (float)y, 80, 20};
            Color bgb = boreActive ? Color{30,50,90,255} : Color{20,30,60,180};
            DrawRectangleRec(bf, bgb);
            DrawRectangleLinesEx(bf, 1,
                boreActive ? Color{100,180,255,255} : Color{60,80,140,200});
            DrawText(boreBuf, (int)bf.x+4, (int)bf.y+4, 11, LIGHTGRAY);

            if (clicked) {
                boreActive = CheckCollisionPointRec(mouse, bf);
            }
            if (boreActive) {
                int key = GetCharPressed();
                int len = (int)strlen(boreBuf);
                while (key > 0) {
                    if (len < 6 && key>='0' && key<='9') {
                        boreBuf[len]=(char)key; boreBuf[len+1]='\0';
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
                    boreBuf[len-1] = '\0';
                if (IsKeyPressed(KEY_ENTER)) {
                    float newC = (float)atof(boreBuf);
                    if (newC != C_bore_ppm) {
                        C_bore_ppm = std::max(0.0f, std::min(newC, 2500.0f));
                        boreChanged = true;
                    }
                    boreActive = false;
                }
            }

            // Info αBore
            snprintf(buf, sizeof(buf),
                     "  alpha_bore ≈ -10 pcm/ppm  →  %.0f pcm",
                     -10.0f * C_bore_ppm);
            DrawText(buf, PX+218, y+4, 10, {140,140,180,200});
            y += 28;
        }

        // ── Courbe historique Xe(t) / I(t) ──────────────────
        _sectionTitle("Historique temporel", PX+10, y);
        y += 18;

        const int CW = PW-20, CH = 100;
        DrawRectangle(PX+10, y, CW, CH, {10,8,20,220});
        DrawRectangleLines(PX+10, y, CW, CH, {50,40,80,180});

        if (history.size() >= 2) {
            // Axes : t sur X, rho_xe sur Y
            float t_min = history.front().t;
            float t_max = history.back().t;
            float rho_min = -35000.0f, rho_max = 0.0f;
            // Adapter l'axe Y au contenu
            for (auto& p : history) {
                rho_min = std::min(rho_min, p.rho_pcm - 500.0f);
            }
            rho_min = std::max(rho_min, -35000.0f);

            float dt = t_max - t_min + 1e-6f;
            float dr = rho_max - rho_min + 1e-6f;

            auto tX = [&](float t) {
                return PX+10 + (int)((t - t_min) / dt * CW);
            };
            auto rY = [&](float r) {
                return y + CH - (int)((r - rho_min) / dr * CH);
            };

            // Grille Y = -5000, -10000, -20000, -30000
            for (float rg : {-5000.0f, -10000.0f, -20000.0f, -30000.0f}) {
                if (rg > rho_min && rg < rho_max) {
                    int ry = rY(rg);
                    DrawLine(PX+10, ry, PX+10+CW, ry, {50,40,80,120});
                }
            }
            // Zéro
            DrawLine(PX+10, rY(0), PX+10+CW, rY(0), {80,80,80,180});

            // Courbe rho_xe (mauve)
            for (int i=1; i<(int)history.size(); ++i) {
                DrawLine(tX(history[i-1].t), rY(history[i-1].rho_pcm),
                         tX(history[i].t),   rY(history[i].rho_pcm),
                         {180,80,255,220});
            }
            // Courbe puissance (vert) — axe secondaire [0..1] → [0, CH]
            for (int i=1; i<(int)history.size(); ++i) {
                int y0p = y+CH - (int)(history[i-1].power * CH);
                int y1p = y+CH - (int)(history[i].power   * CH);
                DrawLine(tX(history[i-1].t), y0p,
                         tX(history[i].t),   y1p,
                         {80,220,100,180});
            }

            // Légende
            DrawRectangle(PX+12, y+2, 8, 8, {180,80,255,255});
            DrawText("rho_Xe", PX+22, y+2, 9, {180,80,255,255});
            DrawRectangle(PX+80, y+2, 8, 8, {80,220,100,255});
            DrawText("Puissance", PX+90, y+2, 9, {80,220,100,255});

            // Temps affiché
            char tlab[32];
            snprintf(tlab, sizeof(tlab), "t=%.0fh", t_max/3600.0f);
            DrawText(tlab, PX+10+CW-40, y+CH-14, 9, {120,120,160,200});
        } else {
            DrawText("  (en attente de donnees...)",
                     PX+12, y+42, 11, {80,80,100,200});
        }
        y += CH + 8;

        // ── Carte 2D empoisonnement ──────────────────────────
        if (!xm.xe_dSigA2.empty() && y + 60 < PY+PH-30) {
            _sectionTitle("Carte empoisonnement (DSigA2)", PX+10, y);
            y += 16;
            int gridCols = (int)std::sqrt((float)N);
            if (gridCols < 1) gridCols = 1;
            int gridRows = (N + gridCols - 1) / gridCols;
            int cellW = std::min(8, (PW-20) / std::max(gridCols,1));
            int cellH = std::min(8, 40 / std::max(gridRows,1));
            float dSig_max = *std::max_element(
                xm.xe_dSigA2.begin(), xm.xe_dSigA2.end());
            if (dSig_max < 1e-10f) dSig_max = 1e-10f;
            for (int i=0;i<N;++i) {
                int cr = i/gridCols, cc = i%gridCols;
                float v = xm.xe_dSigA2[i] / dSig_max;
                uint8_t r = (uint8_t)(v * 200);
                uint8_t b = (uint8_t)((1.0f-v)*180);
                DrawRectangle(PX+10+cc*cellW, y+cr*cellH,
                              cellW-1, cellH-1, {r,30,b,220});
            }
        }

        // ── Bouton fermer ────────────────────────────────────
        Rectangle closeR={(float)(PX+PW-60),(float)(PY+PH-28),55,22};
        bool cOv = CheckCollisionPointRec(mouse, closeR);
        DrawRectangleRec(closeR, cOv?Color{120,40,40,255}:Color{80,30,30,200});
        DrawRectangleLinesEx(closeR,1,{200,80,80,255});
        DrawText("Fermer",PX+PW-54,PY+PH-24,11,LIGHTGRAY);
        if (clicked && cOv) visible = false;

        return boreChanged;
    }

private:
    void _sectionTitle(const char* t, int x, int y) {
        DrawText(t, x, y, 11, {160,140,220,255});
    }

    void _bar(const char* label, float val, float maxVal,
              Color col, int x, int y, int w, int h)
    {
        float frac = (maxVal > 0) ? std::min(val/maxVal, 1.0f) : 0.0f;
        int bw = std::max(1, (int)(frac * (w - 90)));
        DrawText(label, x, y+2, 11, {160,160,200,255});
        DrawRectangle(x+60, y, w-90, h, {20,20,40,180});
        DrawRectangle(x+60, y, bw,   h, col);
        char nbuf[24];
        snprintf(nbuf, sizeof(nbuf), "%.3e", val);
        DrawText(nbuf, x+w-82, y+2, 10, LIGHTGRAY);
    }
};