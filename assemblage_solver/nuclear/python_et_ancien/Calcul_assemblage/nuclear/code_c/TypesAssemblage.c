#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "Grid.h"
#include "TypesAssemblage.h"

/* Palette couleur ASCII/Unicode */
const char *palette[16] = {
    "🟥","🟧","🟨","🟩","🟦","🟪","⬛","⬜",
    "🟫","🟧","🟦","🟩","🟪","🟥","🟨","⬜"
};

/* Couleur associée à un symbole */
int couleur_type(char c) {
    if (c == '-') return -1;
    return ((unsigned char)c) % 16;
}

/* Définition des types : symbole, puissance, stock */
void definir_types(TypeAssemblage *types, int *nb_types) {
    printf("Combien de types d’assemblages ? ");
    scanf("%d", nb_types);

    for (int i = 0; i < *nb_types; i++) {
        printf("Symbole du type %d : ", i+1);
        scanf(" %c", &types[i].symbole);

        printf("Puissance thermique du type %c : ", types[i].symbole);
        scanf("%lf", &types[i].puissance);

        printf("Stock total disponible pour %c : ", types[i].symbole);
        scanf("%d", &types[i].stock_max);
    }
}

/* Remplissage aléatoire simple (ancienne version) */
void remplir_grille_aleatoire(Grid *G, TypeAssemblage *types, int nb_types) {
    for (int i = 0; i < G->size; i++) {
        for (int j = 0; j < G->size; j++) {
            if (!G->core[i][j]) {
                G->g[i][j] = '-';
                continue;
            }
            int k = rand() % nb_types;
            G->g[i][j] = types[k].symbole;
        }
    }
}

/* NOUVEAU : remplissage avec quotas + respect des stocks */
void remplir_grille_avec_quotas(Grid *G, TypeAssemblage *types, int nb_types, int equitable) {
    int n = G->size;

    /* Nombre total de cases du cœur */
    int total = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (G->core[i][j])
                total++;

    int quota = total / nb_types;

    /* Copie des stocks */
    int *restant = malloc(nb_types * sizeof(int));
    for (int k = 0; k < nb_types; k++)
        restant[k] = types[k].stock_max;

    /* Liste des symboles à placer */
    char *liste = malloc(total * sizeof(char));
    int idx = 0;

    /* Mode équitable : quotas uniformes */
    if (equitable) {
        for (int k = 0; k < nb_types; k++) {
            int a_poser = quota;
            if (a_poser > restant[k]) a_poser = restant[k];
            for (int t = 0; t < a_poser; t++)
                liste[idx++] = types[k].symbole;
            restant[k] -= a_poser;
        }

        /* Compléter avec les stocks restants */
        while (idx < total) {
            int k = rand() % nb_types;
            if (restant[k] > 0) {
                liste[idx++] = types[k].symbole;
                restant[k]--;
            }
        }

    } else {
        /* Mode non équitable : tirage pondéré par les stocks */
        while (idx < total) {
            int k = rand() % nb_types;
            if (restant[k] > 0) {
                liste[idx++] = types[k].symbole;
                restant[k]--;
            }
        }
    }

    /* Mélange Fisher–Yates */
    for (int i = total - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        char tmp = liste[i];
        liste[i] = liste[j];
        liste[j] = tmp;
    }

    /* Placement dans la grille */
    idx = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (G->core[i][j])
                G->g[i][j] = liste[idx++];
            else
                G->g[i][j] = '-';

    free(restant);
    free(liste);
}

/* ========================= */
/*   CALCULS THERMIQUES      */
/* ========================= */

void calculer_carte_thermique(Grid *G, TypeAssemblage *types, int nb_types, double **T) {
    for (int i = 0; i < G->size; i++) {
        for (int j = 0; j < G->size; j++) {
            if (!G->core[i][j]) {
                T[i][j] = 0.0;
                continue;
            }
            char c = G->g[i][j];
            double p = 0.0;
            for (int k = 0; k < nb_types; k++)
                if (types[k].symbole == c)
                    p = types[k].puissance;
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
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (!G->core[i][j]) {
                    tmp[i][j] = 0.0;
                    continue;
                }
                double s = T[i][j];
                int cnt = 1;
                if (i > 0 && G->core[i-1][j]) { s += T[i-1][j]; cnt++; }
                if (i < n-1 && G->core[i+1][j]) { s += T[i+1][j]; cnt++; }
                if (j > 0 && G->core[i][j-1]) { s += T[i][j-1]; cnt++; }
                if (j < n-1 && G->core[i][j+1]) { s += T[i][j+1]; cnt++; }
                tmp[i][j] = s / cnt;
            }
        }

        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                T[i][j] = tmp[i][j];
    }

    for (int i = 0; i < n; i++)
        free(tmp[i]);
    free(tmp);
}

void evaluer_thermique(Grid *G, double **T, double *Tmin, double *Tmax, double *deltaT, double *grad_max) {
    *Tmin = 1e9;
    *Tmax = -1e9;
    *grad_max = 0.0;

    int n = G->size;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) continue;

            double t = T[i][j];
            if (t < *Tmin) *Tmin = t;
            if (t > *Tmax) *Tmax = t;

            if (i < n-1 && G->core[i+1][j])
                if (fabs(T[i][j] - T[i+1][j]) > *grad_max)
                    *grad_max = fabs(T[i][j] - T[i+1][j]);

            if (j < n-1 && G->core[i][j+1])
                if (fabs(T[i][j] - T[i][j+1]) > *grad_max)
                    *grad_max = fabs(T[i][j] - T[i][j+1]);
        }
    }

    *deltaT = *Tmax - *Tmin;
}

void afficher_thermique_ascii(Grid *G, double **T) {
    int n = G->size;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) { printf("  "); continue; }
            double t = T[i][j];
            char c =
                (t < 1.0) ? '.' :
                (t < 2.0) ? ':' :
                (t < 3.0) ? '-' :
                (t < 4.0) ? '=' :
                (t < 5.0) ? '+' :
                (t < 6.0) ? '*' :
                (t < 7.0) ? '#' : '%';
            printf("%c ", c);
        }
        printf("\n");
    }
}

void afficher_thermique_couleur(Grid *G, double **T) {
    int n = G->size;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) { printf("⬜"); continue; }
            double t = T[i][j];
            const char *c =
                (t < 1.0) ? "🔵" :
                (t < 2.0) ? "🔹" :
                (t < 3.0) ? "🟦" :
                (t < 4.0) ? "🟩" :
                (t < 5.0) ? "🟨" :
                (t < 6.0) ? "🟧" :
                (t < 7.0) ? "🟥" : "🟪";
            printf("%s", c);
        }
        printf("\n");
    }
}

void power_field(Grid *G, TypeAssemblage *types, int nb_types, double **P) {
    for (int i = 0; i < G->size; i++) {
        for (int j = 0; j < G->size; j++) {
            if (!G->core[i][j]) {
                P[i][j] = 0.0;
                continue;
            }
            char c = G->g[i][j];
            double p = 0.0;
            for (int k = 0; k < nb_types; k++)
                if (types[k].symbole == c)
                    p = types[k].puissance;
            P[i][j] = p;
        }
    }
}
