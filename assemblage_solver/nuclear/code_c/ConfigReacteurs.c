#include "ConfigReacteurs.h"

void charger_config_standard(int choix, ParametresReacteur *P) {

    switch (choix) {

        /* ── 1 : REP ─────────────────────────────────────── */
        case 1:
            P->nb_types = 3;
            P->symboles[0]='A'; P->puissances[0]=1.0;
            P->combustibles[0]=COMB_U235; P->enrichissements[0]=4.0;
            P->symboles[1]='B'; P->puissances[1]=2.0;
            P->combustibles[1]=COMB_U235; P->enrichissements[1]=4.0;
            P->symboles[2]='C'; P->puissances[2]=3.0;
            P->combustibles[2]=COMB_U235; P->enrichissements[2]=4.0;
            /* Thermique REP standard */
            P->therm.temp_entree   = 286.0;
            P->therm.temp_sortie   = 324.0;
            P->therm.moderateur    = 1.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_EAU;
            break;

        /* ── 2 : CANDU ───────────────────────────────────── */
        case 2:
            P->nb_types = 2;
            P->symboles[0]='A'; P->puissances[0]=1.2;
            P->combustibles[0]=COMB_U238; P->enrichissements[0]=0.7;
            P->symboles[1]='B'; P->puissances[1]=0.9;
            P->combustibles[1]=COMB_U238; P->enrichissements[1]=0.7;
            P->therm.temp_entree   = 267.0;
            P->therm.temp_sortie   = 310.0;
            P->therm.moderateur    = 1.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_EAU;
            break;

        /* ── 3 : RNR-Na ──────────────────────────────────── */
        case 3:
            P->nb_types = 4;
            P->symboles[0]='A'; P->puissances[0]=1.0;
            P->combustibles[0]=COMB_PU239; P->enrichissements[0]=15.0;
            P->symboles[1]='B'; P->puissances[1]=1.3;
            P->combustibles[1]=COMB_PU239; P->enrichissements[1]=15.0;
            P->symboles[2]='C'; P->puissances[2]=1.6;
            P->combustibles[2]=COMB_PU239; P->enrichissements[2]=15.0;
            P->symboles[3]='D'; P->puissances[3]=1.9;
            P->combustibles[3]=COMB_PU239; P->enrichissements[3]=15.0;
            P->therm.temp_entree   = 390.0;
            P->therm.temp_sortie   = 550.0;
            P->therm.moderateur    = 0.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_SODIUM;
            break;

        /* ── 4 : RNR-Pb ──────────────────────────────────── */
        case 4:
            P->nb_types = 3;
            P->symboles[0]='A'; P->puissances[0]=1.1;
            P->combustibles[0]=COMB_PU239; P->enrichissements[0]=12.0;
            P->symboles[1]='B'; P->puissances[1]=1.4;
            P->combustibles[1]=COMB_PU239; P->enrichissements[1]=12.0;
            P->symboles[2]='C'; P->puissances[2]=1.7;
            P->combustibles[2]=COMB_PU239; P->enrichissements[2]=12.0;
            P->therm.temp_entree   = 400.0;
            P->therm.temp_sortie   = 480.0;
            P->therm.moderateur    = 0.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_PLOMB_BI;
            break;

        /* ── 5 : Sels fondus thorium ─────────────────────── */
        case 5:
            P->nb_types = 3;
            P->symboles[0]='A'; P->puissances[0]=0.8;
            P->combustibles[0]=COMB_THORIUM; P->enrichissements[0]=1.0;
            P->symboles[1]='B'; P->puissances[1]=1.0;
            P->combustibles[1]=COMB_THORIUM; P->enrichissements[1]=1.0;
            P->symboles[2]='C'; P->puissances[2]=1.2;
            P->combustibles[2]=COMB_THORIUM; P->enrichissements[2]=1.0;
            P->therm.temp_entree   = 565.0;
            P->therm.temp_sortie   = 700.0;
            P->therm.moderateur    = 0.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_HELIUM;  /* approx */
            break;

        /* ── 6 : UNGG ────────────────────────────────────── */
        case 6:
            P->nb_types = 2;
            P->symboles[0]='A'; P->puissances[0]=0.7;
            P->combustibles[0]=COMB_U238; P->enrichissements[0]=0.3;
            P->symboles[1]='B'; P->puissances[1]=0.9;
            P->combustibles[1]=COMB_U238; P->enrichissements[1]=0.3;
            P->therm.temp_entree   = 250.0;
            P->therm.temp_sortie   = 650.0;
            P->therm.moderateur    = 0.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_HELIUM;
            break;

        /* ── 7 : RBMK ────────────────────────────────────── */
        case 7:
            P->nb_types = 3;
            P->symboles[0]='A'; P->puissances[0]=1.0;
            P->combustibles[0]=COMB_U235; P->enrichissements[0]=2.0;
            P->symboles[1]='B'; P->puissances[1]=1.2;
            P->combustibles[1]=COMB_U235; P->enrichissements[1]=2.0;
            P->symboles[2]='C'; P->puissances[2]=1.4;
            P->combustibles[2]=COMB_U235; P->enrichissements[2]=2.0;
            P->therm.temp_entree   = 270.0;
            P->therm.temp_sortie   = 284.0;
            P->therm.moderateur    = 1.0;
            P->therm.puissance_pct = 100.0;
            P->therm.caloporteur   = CALOP_EAU;
            break;

        default:
            P->nb_types = 0;
            break;
    }
}