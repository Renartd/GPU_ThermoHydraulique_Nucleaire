#ifndef THERMIQUE_H
#define THERMIQUE_H

#include "Grid.h"
#include "TypesAssemblage.h"

typedef enum {
    CRITERE_DELTA_T = 1,
    CRITERE_GRADIENT,
    CRITERE_VARIANCE,
    CRITERE_COMBINE
} CritereThermique;

typedef struct {
    CritereThermique critere;
    double poids_deltaT;
    double poids_gradient;
    double poids_variance;
} ParametresThermiques;

void calculer_carte_thermique(Grid *G, TypeAssemblage *types, int nb_types, double **T);
void diffusion_thermique(Grid *G, double **T, int iterations);
void evaluer_thermique(Grid *G, double **T, double *Tmin, double *Tmax, double *deltaT, double *grad_max);

void afficher_thermique_ascii(Grid *G, double **T);
void afficher_thermique_couleur(Grid *G, double **T);

#endif
