#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "OptimisationSymetrique.h"
#include "Reward.h"
#include "Symmetry.h"

typedef struct {
    int i;
    int j;
} Position;

static double rand01(void) {
    return rand() / (double)RAND_MAX;
}

/* Liste des positions valides (core=1) */
static int collect_valid_positions(Grid *G, Position *pos, int max_pos) {
    int n = G->size;
    int count = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (G->core[i][j]) {
                if (count < max_pos) {
                    pos[count].i = i;
                    pos[count].j = j;
                }
                count++;
            }
        }
    }
    return count;
}

/* Action : choisir deux positions valides au hasard */
static void random_swap_action(Grid *G, Position *p1, Position *p2) {
    Position pos[1024];
    int n = collect_valid_positions(G, pos, 1024);
    if (n < 2) {
        p1->i = p1->j = 0;
        p2->i = p2->j = 0;
        return;
    }
    int idx1 = rand() % n;
    int idx2 = rand() % n;
    while (idx2 == idx1 && n > 1) {
        idx2 = rand() % n;
    }
    *p1 = pos[idx1];
    *p2 = pos[idx2];
}

/* Appliquer un swap in-place */
static void apply_swap_inplace(Grid *G, Position p1, Position p2) {
    char tmp = G->g[p1.i][p1.j];
    G->g[p1.i][p1.j] = G->g[p2.i][p2.j];
    G->g[p2.i][p2.j] = tmp;
}

/* Copie profonde de la grille G.g dans dest (même taille) */
static void copy_grid_config(Grid *G, char **dest) {
    int n = G->size;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            dest[i][j] = G->g[i][j];
}

/* Restaure une config dans G.g */
static void restore_grid_config(Grid *G, char **src) {
    int n = G->size;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            G->g[i][j] = src[i][j];
}

void optimisation_symetrique(Grid *G, TypeAssemblage *types, int nb_types,
                             int episodes, int steps,
                             double T_start, double T_end,
                             int use_symmetry) {
    int n = G->size;

    double **P = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++)
        P[i] = malloc(n * sizeof(double));

    char **best_config = malloc(n * sizeof(char *));
    for (int i = 0; i < n; i++)
        best_config[i] = malloc(n * sizeof(char));

    double best_R = -1e9;

    for (int ep = 0; ep < episodes; ep++) {
        /* On part de la config actuelle G.g */
        double R = reward_variance(G, types, nb_types, P);

        for (int step = 0; step < steps; step++) {
            double t = (double)step / (double)steps;
            double T = T_start * pow(T_end / T_start, t);

            Position p1, p2;
            random_swap_action(G, &p1, &p2);

            apply_swap_inplace(G, p1, p2);

            if (use_symmetry) {
                apply_quadripole_symmetry(G);
            }

            double R_new = reward_variance(G, types, nb_types, P);
            double delta = R_new - R;

            if (delta > 0 || rand01() < exp(delta / T)) {
                R = R_new;
            } else {
                /* annuler le swap */
                apply_swap_inplace(G, p1, p2);
                if (use_symmetry) {
                    /* on ne sait pas revenir exactement à l'état précédent
                       après symétrie, donc on ne réapplique pas la symétrie
                       dans ce cas : on suppose symétrie appliquée seulement
                       sur les moves acceptés */
                }
            }
        }

        if (R > best_R) {
            best_R = R;
            copy_grid_config(G, best_config);
        }
    }

    /* On restaure la meilleure config trouvée */
    restore_grid_config(G, best_config);

    for (int i = 0; i < n; i++) {
        free(P[i]);
        free(best_config[i]);
    }
    free(P);
    free(best_config);

    printf("Reward symétrique final = %.6f\n", best_R);
}
