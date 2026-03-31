#ifndef GRID_H
#define GRID_H

typedef struct {
    int **core;
    char **g;
    int rayon;
    int size;
} Grid;

int **alloc_int_grid(int n);
char **alloc_char_grid(int n);
void free_int_grid(int **grid, int n);
void free_char_grid(char **grid, int n);

Grid *generer_grille_circulaire(int rayon);

void afficher_core(Grid *G);
void afficher_grille(Grid *G);

#endif
