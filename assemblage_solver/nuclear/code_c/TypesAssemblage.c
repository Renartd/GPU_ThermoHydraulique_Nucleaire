#include <stdio.h>
#include <string.h>
#include "TypesAssemblage.h"
#include "Grid.h"
#include "ConfigReacteurs.h"

/* ── Lecture ligne (vide le buffer stdin) ──────────────── */
static void lire_ligne(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* ── Nom du combustible ─────────────────────────────────── */
static const char *nom_combustible(CombustibleType c) {
    switch (c) {
        case COMB_THORIUM: return "Thorium";
        case COMB_U238:    return "U238";
        case COMB_U235:    return "U235";
        case COMB_PU239:   return "Pu239";
        case COMB_MOX:     return "MOX";
        default:           return "Inconnu";
    }
}

/* ── Nom du caloporteur ─────────────────────────────────── */
static const char *nom_caloporteur(CaloporteurType c) {
    switch (c) {
        case CALOP_EAU:      return "EAU";
        case CALOP_SODIUM:   return "SODIUM";
        case CALOP_PLOMB_BI: return "PLOMB_BI";
        case CALOP_HELIUM:   return "HELIUM";
        default:             return "EAU";
    }
}

/* ── Choix combustible (mode personnalisé) ─────────────── */
static CombustibleType choisir_combustible(void) {
    int choix = 0;
    printf("Type de combustible :\n");
    printf(" 1) Thorium (Th-232)\n");
    printf(" 2) Uranium 238 (U-238)\n");
    printf(" 3) Uranium 235 (U-235)\n");
    printf(" 4) Plutonium 239 (Pu-239)\n");
    printf(" 5) MOX (melange Pu/U)\n");
    printf("Votre choix : ");
    if (scanf("%d", &choix) != 1) { lire_ligne(); return COMB_U235; }
    lire_ligne();
    switch (choix) {
        case 1: return COMB_THORIUM;
        case 2: return COMB_U238;
        case 3: return COMB_U235;
        case 4: return COMB_PU239;
        case 5: return COMB_MOX;
        default: return COMB_U235;
    }
}

/* ── Choix caloporteur (mode personnalisé) ─────────────── */
static CaloporteurType choisir_caloporteur(void) {
    int choix = 0;
    printf("Caloporteur :\n");
    printf(" 1) Eau legere (H2O)\n");
    printf(" 2) Sodium (Na)\n");
    printf(" 3) Plomb-Bismuth (Pb-Bi)\n");
    printf(" 4) Helium (He)\n");
    printf("Votre choix : ");
    if (scanf("%d", &choix) != 1) { lire_ligne(); return CALOP_EAU; }
    lire_ligne();
    switch (choix) {
        case 1: return CALOP_EAU;
        case 2: return CALOP_SODIUM;
        case 3: return CALOP_PLOMB_BI;
        case 4: return CALOP_HELIUM;
        default: return CALOP_EAU;
    }
}

/* ── Mode personnalisé complet ──────────────────────────── */
void definir_types(TypeAssemblage *types, int *nb_types,
                   ThermiquesReacteur *therm) {
    int n = 0;
    printf("Nombre de types d'assemblages : ");
    if (scanf("%d", &n) != 1 || n <= 0) {
        printf("Valeur invalide, utilisation de 3 types.\n");
        n = 3;
        lire_ligne();
    } else {
        lire_ligne();
    }

    for (int i = 0; i < n; i++) {
        printf("\n=== Type %d ===\n", i + 1);

        printf("Symbole (caractere) : ");
        if (scanf(" %c", &types[i].symbole) != 1)
            types[i].symbole = 'A' + i;
        lire_ligne();

        printf("Puissance thermique (MW) : ");
        if (scanf("%lf", &types[i].puissance) != 1)
            types[i].puissance = 1.0;
        lire_ligne();

        printf("Stock maximal : ");
        if (scanf("%d", &types[i].stock_max) != 1)
            types[i].stock_max = 100;
        lire_ligne();

        types[i].combustible = choisir_combustible();

        if (types[i].combustible == COMB_MOX) {
            printf("Enrichissement MOX - Pu (%%) : ");
            if (scanf("%lf", &types[i].enrichissement_mox_pu) != 1)
                types[i].enrichissement_mox_pu = 0.0;
            lire_ligne();
            printf("Enrichissement MOX - U235 (%%) : ");
            if (scanf("%lf", &types[i].enrichissement_mox_u235) != 1)
                types[i].enrichissement_mox_u235 = 0.0;
            lire_ligne();
            printf("Enrichissement MOX - U238 (%%) : ");
            if (scanf("%lf", &types[i].enrichissement_mox_u238) != 1)
                types[i].enrichissement_mox_u238 = 0.0;
            lire_ligne();
            types[i].enrichissement_principal = 0.0;
        } else {
            printf("Enrichissement principal (%%) : ");
            if (scanf("%lf", &types[i].enrichissement_principal) != 1)
                types[i].enrichissement_principal = 3.0;
            lire_ligne();
            types[i].enrichissement_mox_pu    = 0.0;
            types[i].enrichissement_mox_u235  = 0.0;
            types[i].enrichissement_mox_u238  = 0.0;
        }
    }
    *nb_types = n;

    /* ── Paramètres thermiques personnalisés ─────────────── */
    printf("\n=== Parametres thermiques ===\n");

    printf("Temperature entree caloporteur (deg C) [286] : ");
    if (scanf("%lf", &therm->temp_entree) != 1) therm->temp_entree = 286.0;
    lire_ligne();

    printf("Temperature sortie caloporteur (deg C) [324] : ");
    if (scanf("%lf", &therm->temp_sortie) != 1) therm->temp_sortie = 324.0;
    lire_ligne();

    printf("Ratio moderateur (1.0 = nominal, 0.0 = absent) [1.0] : ");
    if (scanf("%lf", &therm->moderateur) != 1) therm->moderateur = 1.0;
    lire_ligne();

    printf("Puissance nominale (%%) [100] : ");
    if (scanf("%lf", &therm->puissance_pct) != 1) therm->puissance_pct = 100.0;
    lire_ligne();

    therm->caloporteur = choisir_caloporteur();
}

/* ── Mode standard : types depuis config ─────────────────── */
void definir_types_depuis_config(TypeAssemblage *types, int *nb_types,
                                 ParametresReacteur *P) {
    *nb_types = P->nb_types;
    for (int i = 0; i < P->nb_types; i++) {
        types[i].symbole                  = P->symboles[i];
        types[i].puissance                = P->puissances[i];
        types[i].stock_max                = 999;
        types[i].combustible              = P->combustibles[i];
        types[i].enrichissement_principal = P->enrichissements[i];
        types[i].enrichissement_mox_pu    = 0.0;
        types[i].enrichissement_mox_u235  = 0.0;
        types[i].enrichissement_mox_u238  = 0.0;
    }
}

/* ── Sauvegarde assemblage.txt ──────────────────────────── */
/* Écrit TOUTES les métadonnées lues par le simulateur C++  */
void sauver_assemblage(const char *nom_fichier,
                       Grid *G,
                       TypeAssemblage *types, int nb_types,
                       ThermiquesReacteur *therm,
                       int type_reacteur) {

    FILE *f = fopen(nom_fichier, "w");
    if (!f) { perror("Erreur ouverture fichier assemblage"); return; }

    /* ── Métadonnées simulateur C++ ── */
    fprintf(f, "# ============================================\n");
    fprintf(f, "# Fichier genere par assemblage_solver\n");
    fprintf(f, "# ============================================\n");
    fprintf(f, "#\n");
    fprintf(f, "# --- Parametres reacteur (lus par le simulateur) ---\n");

    /* Type réacteur → ReactorType du simulateur */
    const char *type_str = "REP";
    switch (type_reacteur) {
        case 1: type_str = "REP";    break;
        case 2: type_str = "CANDU";  break;
        case 3: type_str = "RNR_NA"; break;
        case 4: type_str = "RNR_PB"; break;
        case 5: type_str = "RHT";    break;  /* UNGG → RHT (gaz) */
        case 6: type_str = "REP";    break;  /* RBMK → REP (eau bouillante) */
        default: type_str = "REP";   break;
    }
    fprintf(f, "# reacteur=%s\n", type_str);

    /* Enrichissement moyen (premier type comme référence) */
    double enrich_ref = (nb_types > 0) ? types[0].enrichissement_principal : 3.5;
    fprintf(f, "# enrichissement=%.2f\n", enrich_ref);

    /* Modérateur et thermique */
    fprintf(f, "# moderateur=%.2f\n",   therm->moderateur);
    fprintf(f, "# puissance=%.1f\n",    therm->puissance_pct);
    fprintf(f, "# tempEntree=%.1f\n",   therm->temp_entree);
    fprintf(f, "# tempSortie=%.1f\n",   therm->temp_sortie);
    fprintf(f, "# caloporteur=%s\n",    nom_caloporteur(therm->caloporteur));
    fprintf(f, "#\n");

    /* ── Grille ── */
    fprintf(f, "# Grille d'assemblage\n");
    fprintf(f, "%d\n", G->size);
    for (int i = 0; i < G->size; i++) {
        for (int j = 0; j < G->size; j++)
            fputc(G->g[i][j], f);
        fputc('\n', f);
    }

    /* ── Types d'assemblages ── */
    fprintf(f, "\n# Types d'assemblages\n");
    fprintf(f, "%d\n", nb_types);
    fprintf(f, "# symbole puissance stock combustible enrich_principal"
               " mox_pu mox_u235 mox_u238\n");
    for (int i = 0; i < nb_types; i++) {
        TypeAssemblage *t = &types[i];
        fprintf(f, "%c %.6f %d %s %.6f %.6f %.6f %.6f\n",
                t->symbole,
                t->puissance,
                t->stock_max,
                nom_combustible(t->combustible),
                t->enrichissement_principal,
                t->enrichissement_mox_pu,
                t->enrichissement_mox_u235,
                t->enrichissement_mox_u238);
    }

    fclose(f);
    printf("Fichier '%s' ecrit.\n", nom_fichier);
}