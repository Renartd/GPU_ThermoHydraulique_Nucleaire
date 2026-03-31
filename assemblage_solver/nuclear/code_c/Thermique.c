#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "Thermique.h"

void calculer_carte_thermique(Grid *G, TypeAssemblage *types, int nb_types, double **T) {
    int n = G->size;

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            T[i][j] = 0.0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) continue;
            char c = G->g[i][j];
            double p = 0.0;
            for (int k = 0; k < nb_types; k++) {
                if (types[k].symbole == c) {
                    p = types[k].puissance;
                    break;
                }
            }
            T[i][j] = p;
        }
    }
}

void diffusion_thermique(Grid *G, double **T, int iterations) {
    int n = G->size;

    double **tmp = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++)
        tmp[i] = malloc(n * sizeof(double));

    for (int it = 0; it < iterations; it++) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                tmp[i][j] = T[i][j];

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (!G->core[i][j]) continue;

                double s = tmp[i][j];
                int cnt = 1;

                if (i > 0 && G->core[i-1][j]) { s += tmp[i-1][j]; cnt++; }
                if (i < n-1 && G->core[i+1][j]) { s += tmp[i+1][j]; cnt++; }
                if (j > 0 && G->core[i][j-1]) { s += tmp[i][j-1]; cnt++; }
                if (j < n-1 && G->core[i][j+1]) { s += tmp[i][j+1]; cnt++; }

                T[i][j] = s / cnt;
            }
        }
    }

    for (int i = 0; i < n; i++)
        free(tmp[i]);
    free(tmp);
}

void evaluer_thermique(Grid *G, double **T, double *Tmin, double *Tmax, double *deltaT, double *grad_max) {
    int n = G->size;

    *Tmin = 1e9;
    *Tmax = -1e9;
    *grad_max = 0.0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) continue;
            double v = T[i][j];
            if (v < *Tmin) *Tmin = v;
            if (v > *Tmax) *Tmax = v;

            if (i < n-1 && G->core[i+1][j]) {
                double g = fabs(T[i+1][j] - v);
                if (g > *grad_max) *grad_max = g;
            }
            if (j < n-1 && G->core[i][j+1]) {
                double g = fabs(T[i][j+1] - v);
                if (g > *grad_max) *grad_max = g;
            }
        }
    }

    *deltaT = *Tmax - *Tmin;
}

void afficher_thermique_ascii(Grid *G, double **T) {
    int n = G->size;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) {
                printf(" . ");
                continue;
            }
            double v = T[i][j];
            char c = (v < 1.0 ? '-' :
                      v < 2.0 ? '=' :
                      v < 3.0 ? ':' :
                                '#');
            printf(" %c ", c);
        }
        printf("\n");
    }
}

void afficher_thermique_couleur(Grid *G, double **T) {
    int n = G->size;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) {
                printf("⬜");
                continue;
            }
            double v = T[i][j];
            const char *col =
                (v < 1.0 ? "🟦" :
                 v < 2.0 ? "🟩" :
                 v < 3.0 ? "🟨" :
                           "🟥");
            printf("%s", col);
        }
        printf("\n");
    }
}
