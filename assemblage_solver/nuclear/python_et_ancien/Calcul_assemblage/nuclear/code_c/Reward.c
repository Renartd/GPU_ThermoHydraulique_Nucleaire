#include <math.h>
#include "Reward.h"

double reward_variance(Grid *G, TypeAssemblage *types, int nb_types,
                       double **P) {
    int n = G->size;

    power_field(G, types, nb_types, P);

    double total = 0.0;
    double total_sq = 0.0;
    int count = 0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!G->core[i][j]) continue;
            double v = P[i][j];
            if (v > 0.0) {
                total += v;
                total_sq += v * v;
                count++;
            }
        }
    }

    if (count == 0) return 0.0;

    double mean = total / count;
    double var = (total_sq / count) - (mean * mean);

    return -var;
}
