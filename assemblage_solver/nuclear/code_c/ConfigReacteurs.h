#ifndef CONFIG_REACTEURS_H
#define CONFIG_REACTEURS_H

#include "TypesAssemblage.h"

/* ── ParametresReacteur ────────────────────────────────── */
/* Défini UNE SEULE FOIS ici (TypesAssemblage.h le déclare  */
/* en forward declaration pour éviter le doublon).          */
struct ParametresReacteur {
    int    nb_types;
    char   symboles[8];
    double puissances[8];
    CombustibleType combustibles[8];
    double enrichissements[8];
    /* Paramètres thermiques standards selon le type */
    ThermiquesReacteur therm;
};

/* ── Noms des réacteurs ────────────────────────────────── */
static const char *NOMS_REACTEURS[] = {
    "",                      /* 0 — inutilisé */
    "REP",                   /* 1 */
    "CANDU",                 /* 2 */
    "RNR-Na (sodium)",       /* 3 */
    "RNR-Pb (plomb-bismuth)",/* 4 */
    "UNGG",                  /* 5 */
    "RBMK"                   /* 6 */
};

/* ── Fonctions ─────────────────────────────────────────── */
void charger_config_standard(int choix, ParametresReacteur *P);

#endif