#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "Grid.h"
#include "TypesAssemblage.h"
#include "MonteCarlo.h"
#include "Thermique.h"


static double rand01(void) {
    return rand() / (double)RAND_MAX;
}

static void echanger_assemblages(Grid *G, int i1, int j1, int i2, int j2) {
    char tmp = G->g[i1][j1];
    G->g[i1][j1] = G->g[i2][j2];
    G->g[i2][j2] = tmp;
}

void monte_carlo_metropolis(Grid *G, TypeAssemblage *types, int nb_types,
                            int iterations, double T,
                            double **Tfield) {
    double Tmin, Tmax, deltaT, grad_max;

    calculer_carte_thermique(G, types, nb_types, Tfield);
    diffusion_thermique(G, Tfield, 5);
    evaluer_thermique(G, Tfield, &Tmin, &Tmax, &deltaT, &grad_max);

    for (int it = 0; it < iterations; it++) {
        int i1, j1, i2, j2;
        do {
            i1 = rand() % G->size;
            j1 = rand() % G->size;
        } while (!G->core[i1][j1]);

        do {
            i2 = rand() % G->size;
            j2 = rand() % G->size;
        } while (!G->core[i2][j2]);

        echanger_assemblages(G, i1, j1, i2, j2);

        calculer_carte_thermique(G, types, nb_types, Tfield);
        diffusion_thermique(G, Tfield, 5);

        double Tmin2, Tmax2, deltaT2, grad2;
        evaluer_thermique(G, Tfield, &Tmin2, &Tmax2, &deltaT2, &grad2);

        double dE = deltaT2 - deltaT;

        if (dE < 0) {
            deltaT = deltaT2;
        } else {
            double p = exp(-dE / T);
            double r = rand01();
            if (r < p) {
                deltaT = deltaT2;
            } else {
                echanger_assemblages(G, i1, j1, i2, j2);
            }
        }
    }

    calculer_carte_thermique(G, types, nb_types, Tfield);
    diffusion_thermique(G, Tfield, 5);
    evaluer_thermique(G, Tfield, &Tmin, &Tmax, &deltaT, &grad_max);

    printf("Après Monte Carlo :\n");
    printf("Tmin = %.4f\n", Tmin);
    printf("Tmax = %.4f\n", Tmax);
    printf("ΔT   = %.4f\n", deltaT);
    printf("Gradient max = %.4f\n", grad_max);
}

void recuit_simule(Grid *G, TypeAssemblage *types, int nb_types,
                   int iterations, double T0, double alpha,
                   double **Tfield) {
    double Tmin, Tmax, deltaT, grad_max;
    double T = T0;

    calculer_carte_thermique(G, types, nb_types, Tfield);
    diffusion_thermique(G, Tfield, 5);
    evaluer_thermique(G, Tfield, &Tmin, &Tmax, &deltaT, &grad_max);

    for (int it = 0; it < iterations; it++) {
        int i1, j1, i2, j2;
        do {
            i1 = rand() % G->size;
            j1 = rand() % G->size;
        } while (!G->core[i1][j1]);

        do {
            i2 = rand() % G->size;
            j2 = rand() % G->size;
        } while (!G->core[i2][j2]);

        echanger_assemblages(G, i1, j1, i2, j2);

        calculer_carte_thermique(G, types, nb_types, Tfield);
        diffusion_thermique(G, Tfield, 5);

        double Tmin2, Tmax2, deltaT2, grad2;
        evaluer_thermique(G, Tfield, &Tmin2, &Tmax2, &deltaT2, &grad2);

        double dE = deltaT2 - deltaT;

        if (dE < 0) {
            deltaT = deltaT2;
        } else {
            double p = exp(-dE / T);
            double r = rand01();
            if (r < p) {
                deltaT = deltaT2;
            } else {
                echanger_assemblages(G, i1, j1, i2, j2);
            }
        }

        T *= alpha;
    }

    calculer_carte_thermique(G, types, nb_types, Tfield);
    diffusion_thermique(G, Tfield, 5);
    evaluer_thermique(G, Tfield, &Tmin, &Tmax, &deltaT, &grad_max);

    printf("Après recuit simulé :\n");
    printf("Tmin = %.4f\n", Tmin);
    printf("Tmax = %.4f\n", Tmax);
    printf("ΔT   = %.4f\n", deltaT);
    printf("Gradient max = %.4f\n", grad_max);
}
