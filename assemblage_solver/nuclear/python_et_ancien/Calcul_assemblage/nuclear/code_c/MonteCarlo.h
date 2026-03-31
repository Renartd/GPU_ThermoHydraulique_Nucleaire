#ifndef MONTE_CARLO_H
#define MONTE_CARLO_H

void monte_carlo_metropolis(Grid *G, TypeAssemblage *types, int nb_types,
                            int iterations, double T,
                            double **Tfield);

void recuit_simule(Grid *G, TypeAssemblage *types, int nb_types,
                   int iterations, double T0, double alpha,
                   double **Tfield);

#endif
