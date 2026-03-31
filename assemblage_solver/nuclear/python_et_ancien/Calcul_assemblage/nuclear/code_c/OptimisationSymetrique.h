#ifndef OPTIMISATION_SYMETRIQUE_H
#define OPTIMISATION_SYMETRIQUE_H

#include "Grid.h"
#include "TypesAssemblage.h"

/* Recuit simulé "symétrique" basé sur le reward = -variance.
   Utilise des échanges aléatoires et peut appliquer la symétrie quadripolaire. */
void optimisation_symetrique(Grid *G, TypeAssemblage *types, int nb_types,
                             int episodes, int steps,
                             double T_start, double T_end,
                             int use_symmetry);

#endif
