#include "Affichage.h"

/* 8 couleurs distinctes + blanc réservé au vide */
const char *palette[] = {
    "🟥",  // rouge
    "🟧",  // orange
    "🟨",  // jaune
    "🟩",  // vert
    "🟦",  // bleu
    "🟪",  // violet
    "🟫",  // marron
    "⬛",  // noir
};

/* Taille réelle de la palette */
#define PALETTE_SIZE (sizeof(palette)/sizeof(palette[0]))

/*
 * Sélection dynamique :
 * - '-' → blanc (vide)
 * - tout autre symbole → couleur dynamique
 */
int couleur_type(char c) {
    if (c == '-' || c == ' ')
        return -1;  // blanc réservé au vide

    unsigned int code = (unsigned int)c;
    return code % PALETTE_SIZE;
}
