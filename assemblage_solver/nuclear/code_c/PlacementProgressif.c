#include <math.h>
#include <stdlib.h>
#include "PlacementProgressif.h"
#include "Grid.h"
#include "TypesAssemblage.h"

/* ---------------------------------------------------------
   Génère les 8 symétries d’un point (i, j) autour du centre
   --------------------------------------------------------- */
static void symetries_ordre8(int i, int j, int centre, int out[8][2]) {
    int k = 0;

    int coords[8][2] = {
        {  i,  j},
        {  j,  i},
        { -i,  j},
        { -j,  i},
        {  i, -j},
        {  j, -i},
        { -i, -j},
        { -j, -i}
    };

    for (int n = 0; n < 8; n++) {
        out[k][0] = centre + coords[n][0];
        out[k][1] = centre + coords[n][1];
        k++;
    }
}

/* ---------------------------------------------------------
   Placement symétrique EXACTEMENT comme Python
   --------------------------------------------------------- */
void remplir_grille_symetrique(Grid *G, TypeAssemblage *types, int nb_types) {

    int size   = G->size;
    int centre = size / 2;

    /* compteur d’usage par symbole */
    int *usage = calloc(nb_types, sizeof(int));

    /* On parcourt un huitième du disque : i >= 0, j >= i */
    for (int i = 0; i <= centre; i++) {
        for (int j = i; j <= centre; j++) {

            /* Case centrale du huitième */
            int x0 = centre + i;
            int y0 = centre + j;

            if (!G->core[x0][y0])
                continue;

            /* Sélection du type avec pénalité exponentielle */
            int best_index = 0;
            double best_score = INFINITY;

            for (int t = 0; t < nb_types; t++) {
                double penalty = exp((double)usage[t]);
                if (penalty < best_score) {
                    best_score = penalty;
                    best_index = t;
                }
            }

            /* Appliquer les 8 symétries */
            int pts[8][2];
            symetries_ordre8(i, j, centre, pts);

            for (int k = 0; k < 8; k++) {
                int x = pts[k][0];
                int y = pts[k][1];

                if (x >= 0 && x < size && y >= 0 && y < size && G->core[x][y]) {
                    G->g[x][y] = types[best_index].symbole;
                    usage[best_index] += 1;
                }
            }
        }
    }

    free(usage);
}
