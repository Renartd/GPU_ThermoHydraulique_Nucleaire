#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Grid.h"
#include "TypesAssemblage.h"
#include "Thermique.h"
#include "PlacementProgressif.h"
#include "Affichage.h"
#include "ConfigReacteurs.h"

int main(void) {
    srand((unsigned int)time(NULL));

    int mode = 0;
    printf("Mode d'utilisation :\n");
    printf(" 1) Standard (reacteur preconfigure)\n");
    printf(" 2) Personnalise (tout saisir)\n");
    printf("Votre choix : ");
    if (scanf("%d", &mode) != 1) mode = 1;

    int rayon = 5;
    printf("Rayon du coeur : ");
    if (scanf("%d", &rayon) != 1 || rayon <= 0) rayon = 5;

    Grid *G = generer_grille_circulaire(rayon);
    printf("\n=== Grille circulaire (taille = %d) ===\n", G->size);
    afficher_core(G);

    TypeAssemblage   types[32];
    int              nb_types    = 0;
    int              type_reacteur = 0;   /* 1-7 pour standard, 0 = perso */
    ThermiquesReacteur therm;

    /* valeurs par défaut */
    therm.temp_entree   = 286.0;
    therm.temp_sortie   = 324.0;
    therm.moderateur    = 1.0;
    therm.puissance_pct = 100.0;
    therm.caloporteur   = CALOP_EAU;

    /* ── MODE STANDARD ─────────────────────────────────────── */
    if (mode == 1) {
        printf("\nChoisissez un modele standard :\n");
        printf(" 1) REP\n");
        printf(" 2) CANDU\n");
        printf(" 3) RNR sodium\n");
        printf(" 4) RNR plomb\n");
        printf(" 5) UNGG\n");
        printf(" 6) RBMK\n");
        printf("Votre choix : ");
        if (scanf("%d", &type_reacteur) != 1) type_reacteur = 1;

        ParametresReacteur P;
        charger_config_standard(type_reacteur, &P);

        if (P.nb_types == 0) {
            printf("Modele inconnu. Mode personnalise.\n");
            definir_types(types, &nb_types, &therm);
            type_reacteur = 0;
        } else {
            definir_types_depuis_config(types, &nb_types, &P);
            therm = P.therm;   /* paramètres thermiques standards */

            printf("\nConfiguration %s chargee (%d types).\n",
                   NOMS_REACTEURS[type_reacteur], nb_types);
            printf("  Caloporteur : %s\n",
                   (therm.caloporteur == CALOP_EAU)      ? "Eau (H2O)"     :
                   (therm.caloporteur == CALOP_SODIUM)   ? "Sodium"        :
                   (therm.caloporteur == CALOP_PLOMB_BI) ? "Plomb-Bismuth" :
                                                           "Helium");
            printf("  T entree : %.0f°C  T sortie : %.0f°C\n",
                   therm.temp_entree, therm.temp_sortie);
            printf("  Moderateur : %.1f\n", therm.moderateur);

            /* Nb types ajustable */
            printf("\nNombre de types souhaite : ");
            int n2 = 0;
            if (scanf("%d", &n2) == 1 && n2 > 0 && n2 <= 8) {
                for (int i = P.nb_types; i < n2; i++) {
                    types[i].symbole = 'A' + i;
                    types[i].puissance = 1.0;
                    types[i].stock_max = 999;
                    types[i].combustible = COMB_U235;
                    types[i].enrichissement_principal = 3.0;
                    types[i].enrichissement_mox_pu    = 0.0;
                    types[i].enrichissement_mox_u235  = 0.0;
                    types[i].enrichissement_mox_u238  = 0.0;
                }
                nb_types = n2;
            }

            /* Puissances ajustables */
            printf("\nSaisir la puissance de chaque type :\n");
            for (int i = 0; i < nb_types; i++) {
                printf("Puissance type %c (MW) : ", types[i].symbole);
                double val;
                if (scanf("%lf", &val) == 1 && val > 0)
                    types[i].puissance = val;
            }
        }
    }

    /* ── MODE PERSONNALISE ─────────────────────────────────── */
    else {
        definir_types(types, &nb_types, &therm);
        type_reacteur = 0;
    }

    /* ── PLACEMENT ─────────────────────────────────────────── */
    remplir_grille_symetrique(G, types, nb_types);

    /* ── THERMIQUE 2D INTERNE ──────────────────────────────── */
    int n = G->size;
    double **T = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++)
        T[i] = malloc(n * sizeof(double));

    double Tmin, Tmax, deltaT, grad_max;
    calculer_carte_thermique(G, types, nb_types, T);
    diffusion_thermique(G, T, 40);
    evaluer_thermique(G, T, &Tmin, &Tmax, &deltaT, &grad_max);

    printf("\n=== Evaluation thermique 2D ===\n");
    printf("  Tmin = %.4f  Tmax = %.4f\n", Tmin, Tmax);
    printf("  DeltaT = %.4f  Gradient max = %.4f\n", deltaT, grad_max);

    /* ── AFFICHAGE ─────────────────────────────────────────── */
    printf("\n=== Legende ===\n");
    for (int t = 0; t < nb_types; t++) {
        int idx = couleur_type(types[t].symbole);
        printf("  %c → %s\n", types[t].symbole,
               idx == -1 ? "⬜" : palette[idx]);
    }

    printf("\n=== Grille ASCII ===\n");
    for (int i = 0; i < G->size; i++) {
        for (int j = 0; j < G->size; j++)
            printf("%c ", G->g[i][j]);
        printf("\n");
    }

    printf("\n=== Grille couleurs ===\n");
    afficher_grille(G);

    /* ── EXPORT ────────────────────────────────────────────── */
    /* Chemin de sortie : calcul3d/data/Assemblage.txt          */
    /* (relatif au dossier de lancement de l'orchestrateur)     */
    const char *chemin_sortie = "../../../calcul3d/data/Assemblage.txt";

    printf("\nSauvegarde dans %s...\n", chemin_sortie);
    sauver_assemblage(chemin_sortie, G, types, nb_types,
                      &therm, type_reacteur);

    /* Nettoyage */
    for (int i = 0; i < n; i++) free(T[i]);
    free(T);
    for (int i = 0; i < G->size; i++) {
        free(G->core[i]);
        free(G->g[i]);
    }
    free(G->core);
    free(G->g);
    free(G);

    return 0;
}
