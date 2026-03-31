#ifndef REWARD_H
#define REWARD_H

#include "Grid.h"
#include "TypesAssemblage.h"

/* Calcule le reward = -variance du champ de puissance brut (sans diffusion). */
double reward_variance(Grid *G, TypeAssemblage *types, int nb_types,
                       double **P);

#endif
