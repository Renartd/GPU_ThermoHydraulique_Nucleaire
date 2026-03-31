#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "Grid.h"
#include "TypesAssemblage.h"
#include "Affichage.h"

// ===============================
// Allocations
// ===============================

int **alloc_int_grid(int n) {
    int **m = malloc(n * sizeof(int *));
    for(int i = 0; i < n; i++)
        m[i] = calloc(n, sizeof(int));
    return m;
}

char **alloc_char_grid(int n) {
    char **m = malloc(n * sizeof(char *));
    for(int i = 0; i < n; i++) {
        m[i] = malloc(n * sizeof(char));
        for (int j = 0; j < n; j++)
            m[i][j] = '-';
    }
    return m;
}

void free_int_grid(int **grid, int n) {
    for(int i = 0; i < n; i++)
        free(grid[i]);
    free(grid);
}

void free_char_grid(char **grid, int n) {
    for(int i = 0; i < n; i++)
        free(grid[i]);
    free(grid);
}

// ===============================
// 1) Cercle euclidien
// ===============================

void generer_cercle(Grid *G) {
    int n  = G->size;
    int cx = G->rayon;
    int cy = G->rayon;

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            G->core[i][j] = 0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int dx = i - cx;
            int dy = j - cy;
            if (dx*dx + dy*dy <= G->rayon * G->rayon)
                G->core[i][j] = 1;
        }
    }
}

// ===============================
// 2) Correction des extrémités
// ===============================

void corriger_extremites(Grid *G) {
    int n  = G->size;
    int cx = G->rayon;
    int cy = G->rayon;

    // Haut
    int hx = cx - G->rayon;
    int hy = cy;
    if (hx >= 0) {
        if (hy > 0)     G->core[hx][hy-1] = 1;
        G->core[hx][hy] = 1;
        if (hy < n - 1) G->core[hx][hy+1] = 1;
    }

    // Bas
    int bx = cx + G->rayon;
    int by = cy;
    if (bx < n) {
        if (by > 0)     G->core[bx][by-1] = 1;
        G->core[bx][by] = 1;
        if (by < n - 1) G->core[bx][by+1] = 1;
    }

    // Gauche
    int gx = cx;
    int gy = cy - G->rayon;
    if (gy >= 0) {
        if (gx > 0)     G->core[gx-1][gy] = 1;
        G->core[gx][gy] = 1;
        if (gx < n - 1) G->core[gx+1][gy] = 1;
    }

    // Droite
    int dx2 = cx;
    int dy2 = cy + G->rayon;
    if (dy2 < n) {
        if (dx2 > 0)     G->core[dx2-1][dy2] = 1;
        G->core[dx2][dy2] = 1;
        if (dx2 < n - 1) G->core[dx2+1][dy2] = 1;
    }
}

// ===============================
// 3) Génération complète
// ===============================

Grid *generer_grille_circulaire(int rayon) {
    Grid *G = malloc(sizeof(Grid));
    G->rayon = rayon;
    G->size  = 2 * rayon + 1;

    int n = G->size;

    G->core = alloc_int_grid(n);
    G->g    = alloc_char_grid(n);

    generer_cercle(G);
    corriger_extremites(G);

    return G;
}

// ===============================
// Affichages
// ===============================

void afficher_core(Grid *G) {
    for(int i = 0; i < G->size; i++) {
        for(int j = 0; j < G->size; j++)
            printf("%c ", G->core[i][j] ? 'O' : '.');
        printf("\n");
    }
}

void afficher_grille(Grid *G) {
    for (int i = 0; i < G->size; i++) {
        for (int j = 0; j < G->size; j++) {
            char c = G->g[i][j];

            if (c == '-' || !G->core[i][j]) {
                printf("⬜");  // blanc pour vide
                continue;
            }

            int idx = couleur_type(c);
            printf("%s", palette[idx]);
        }
        printf("\n");
    }
}

