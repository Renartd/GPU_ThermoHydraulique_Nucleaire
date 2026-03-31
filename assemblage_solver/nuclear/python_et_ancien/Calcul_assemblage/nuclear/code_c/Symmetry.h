#ifndef SYMMETRY_H
#define SYMMETRY_H

#include "Grid.h"

/* Applique la symétrie quadripolaire sur la grille G.g,
   en respectant le masque G.core. */
void apply_quadripole_symmetry(Grid *G);

#endif
