#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "Grid.h"
#include "TypesAssemblage.h"
#include "MonteCarlo.h"
#include "PlacementProgressif.h"

static double **alloc_double_grid(int n) {
    double **m = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++)
        m[i] = malloc(n * sizeof(double));
    return m;
}

static void free_double_grid(double **m, int n) {
    for (int i = 0; i < n; i++)
        free(m[i]);
    free(m);
}

static void evaluer_et_afficher(Grid *G, TypeAssemblage *types, int nb_types, double **Tfield) {
    double Tmin, Tmax, deltaT, grad_max;

    calculer_carte_thermique(G, types, nb_types, Tfield);
    diffusion_thermique(G, Tfield, 5);
    evaluer_thermique(G, Tfield, &Tmin, &Tmax, &deltaT, &grad_max);

    printf("\n=== Carte thermique (ASCII) ===\n");
    afficher_thermique_ascii(G, Tfield);

    printf("\n=== Carte thermique (couleur) ===\n");
    afficher_thermique_couleur(G, Tfield);

    printf("\n=== Évaluation ===\n");
    printf("Tmin = %.4f\n", Tmin);
    printf("Tmax = %.4f\n", Tmax);
    printf("ΔT   = %.4f\n", deltaT);
    printf("Gradient max = %.4f\n", grad_max);
}

int main(void) {
    srand((unsigned int)time(NULL));

    int rayon;
    printf("Rayon du cœur circulaire (max 25) : ");
    if (scanf("%d", &rayon) != 1) return 1;

    Grid G = generer_grille_circulaire(rayon);

    printf("\n=== Cœur généré (cercle) ===\n");
    afficher_core(G);

    TypeAssemblage types[16];
    int nb_types = 0;
    definir_types(types, &nb_types);

    double **Tfield = alloc_double_grid(G.size);

    int choix = -1;
    while (1) {
        printf("\n=== Menu optimisation ===\n");
        printf("1) Juste aléatoire + évaluation\n");
        printf("2) Monte Carlo (Metropolis)\n");
        printf("3) Recuit simulé\n");
        printf("6) Placement progressif thermique (symétrie quadripolaire)\n");
        printf("0) Quitter\n");
        printf("Votre choix : ");
        if (scanf("%d", &choix) != 1) break;
        if (choix == 0) break;

        if (choix == 1 || choix == 2 || choix == 3) {
            int equitable;
            printf("Tirage équitable ? (1 = oui, 0 = non) : ");
            scanf("%d", &equitable);

            remplir_grille_avec_quotas(&G, types, nb_types, equitable);

            printf("\n=== Remplissage initial ===\n");
            afficher_grille(G);
        }

        if (choix == 1) {
            evaluer_et_afficher(&G, types, nb_types, Tfield);

        } else if (choix == 2) {
            monte_carlo_metropolis(&G, types, nb_types, 2000, 0.5, Tfield);
            afficher_grille(G);
            evaluer_et_afficher(&G, types, nb_types, Tfield);

        } else if (choix == 3) {
            recuit_simule(&G, types, nb_types, 5000, 1.0, 0.999, Tfield);
            afficher_grille(G);
            evaluer_et_afficher(&G, types, nb_types, Tfield);

        } else if (choix == 6) {
            for (int i = 0; i < G.size; i++)
                for (int j = 0; j < G.size; j++)
                    G.g[i][j] = '-';

            ParametresThermiques thermo;
            int c;
            printf("Choix du critère thermique :\n");
            printf("1) ΔT\n");
            printf("2) Gradient\n");
            printf("3) Variance\n");
            printf("4) Combinaison (ΔT + gradient + variance)\n");
            printf("Votre choix : ");
            scanf("%d", &c);
            thermo.critere = (CritereThermique)c;
            thermo.poids_deltaT = 1.0;
            thermo.poids_gradient = 1.0;
            thermo.poids_variance = 1.0;
            if (c == 4) {
                printf("Poids pour ΔT : ");
                scanf("%lf", &thermo.poids_deltaT);
                printf("Poids pour gradient : ");
                scanf("%lf", &thermo.poids_gradient);
                printf("Poids pour variance : ");
                scanf("%lf", &thermo.poids_variance);
            }

            placement_progressif(&G, types, nb_types, thermo);

            printf("\n=== Grille après placement progressif ===\n");
            afficher_grille(G);
            evaluer_et_afficher(&G, types, nb_types, Tfield);

        } else {
            printf("Choix invalide.\n");
        }
    }

    free_double_grid(Tfield, G.size);
    free_int_grid(G.core, G.size);
    free_char_grid(G.g, G.size);

    return 0;
}
