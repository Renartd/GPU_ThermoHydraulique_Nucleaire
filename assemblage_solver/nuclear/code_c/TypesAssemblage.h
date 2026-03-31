#ifndef TYPES_ASSEMBLAGE_H
#define TYPES_ASSEMBLAGE_H

#include "Grid.h"

/* ── Combustibles ──────────────────────────────────────── */
typedef enum {
    COMB_THORIUM,
    COMB_U238,
    COMB_U235,
    COMB_PU239,
    COMB_MOX
} CombustibleType;

/* ── Type de caloporteur (pour simulateur C++) ─────────── */
typedef enum {
    CALOP_EAU,
    CALOP_SODIUM,
    CALOP_PLOMB_BI,
    CALOP_HELIUM
} CaloporteurType;

/* ── Type d'assemblage ─────────────────────────────────── */
typedef struct {
    char   symbole;
    double puissance;
    int    stock_max;
    CombustibleType combustible;
    double enrichissement_principal;
    double enrichissement_mox_pu;
    double enrichissement_mox_u235;
    double enrichissement_mox_u238;
} TypeAssemblage;

/* ── Paramètres thermiques du réacteur ─────────────────── */
/* Transmis au simulateur C++ via assemblage.txt           */
typedef struct {
    double temp_entree;      /* °C — température entrée caloporteur */
    double temp_sortie;      /* °C — température sortie caloporteur */
    double moderateur;       /* ratio modérateur (1.0 = nominal)    */
    double puissance_pct;    /* % puissance nominale                */
    CaloporteurType caloporteur;
} ThermiquesReacteur;

/* ── Fonctions ─────────────────────────────────────────── */
void definir_types(TypeAssemblage *types, int *nb_types,
                   ThermiquesReacteur *therm);

void sauver_assemblage(const char *nom_fichier,
                       Grid *G,
                       TypeAssemblage *types, int nb_types,
                       ThermiquesReacteur *therm,
                       int type_reacteur);

/* Déclaration anticipée — évite le doublon avec ConfigReacteurs.h */
typedef struct ParametresReacteur ParametresReacteur;
void definir_types_depuis_config(TypeAssemblage *types, int *nb_types,
                                 ParametresReacteur *P);

#endif