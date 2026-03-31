from dataclasses import dataclass
from typing import List, Callable
from types_assemblage import TypeAssemblage

import random

@dataclass
class Grid:
    """
    Structure représentant une grille d'assemblage.

    Attributes
    ----------
    core : List[List[int]]
        Matrice indiquant le cœur (1) ou vide (0).
    g : List[List[str]]
        Matrice des symboles d’assemblage ou '-'.
    rayon : int
        Rayon du cœur.
    size : int
        Taille de la grille (2*rayon+1).
    """
    core: List[List[int]]
    g: List[List[str]]
    rayon: int
    size: int

def alloc_int_grid(n: int) -> List[List[int]]:
    """
    Alloue une grille d'entiers initialisée à 0.
    """
    return [[0 for _ in range(n)] for _ in range(n)]

def alloc_char_grid(n: int) -> List[List[str]]:
    """
    Alloue une grille de caractères initialisée à '-'.
    """
    return [['-' for _ in range(n)] for _ in range(n)]

def generer_cercle(G: Grid) -> None:
    """
    Génère un cercle euclidien dans la grille.
    """
    n = G.size
    cx = G.rayon
    cy = G.rayon
    for i in range(n):
        for j in range(n):
            G.core[i][j] = 0
    for i in range(n):
        for j in range(n):
            dx = i - cx
            dy = j - cy
            if dx*dx + dy*dy <= G.rayon * G.rayon:
                G.core[i][j] = 1

def corriger_extremites(G: Grid) -> None:
    """
    Corrige les extrémités du cercle (épaissit les sommets).
    """
    n = G.size
    cx = G.rayon
    cy = G.rayon

    # Haut
    hx = cx - G.rayon
    hy = cy
    if hx >= 0:
        if hy > 0:     G.core[hx][hy-1] = 1
        G.core[hx][hy] = 1
        if hy < n - 1: G.core[hx][hy+1] = 1

    # Bas
    bx = cx + G.rayon
    by = cy
    if bx < n:
        if by > 0:     G.core[bx][by-1] = 1
        G.core[bx][by] = 1
        if by < n - 1: G.core[bx][by+1] = 1

    # Gauche
    gx = cx
    gy = cy - G.rayon
    if gy >= 0:
        if gx > 0:     G.core[gx-1][gy] = 1
        G.core[gx][gy] = 1
        if gx < n - 1: G.core[gx+1][gy] = 1

    # Droite
    dx2 = cx
    dy2 = cy + G.rayon
    if dy2 < n:
        if dx2 > 0:     G.core[dx2-1][dy2] = 1
        G.core[dx2][dy2] = 1
        if dx2 < n - 1: G.core[dx2+1][dy2] = 1

def generer_grille_circulaire(rayon: int) -> Grid:
    """
    Génère une grille circulaire.

    Parameters
    ----------
    rayon : int
        Rayon du cœur.

    Returns
    -------
    Grid
        Grille générée.
    """
    size = 2 * rayon + 1
    core = alloc_int_grid(size)
    g = alloc_char_grid(size)
    G = Grid(core=core, g=g, rayon=rayon, size=size)
    generer_cercle(G)
    corriger_extremites(G)
    return G

def remplir_grille_aleatoire(G: Grid, types: List['TypeAssemblage'], nb_types: int) -> None:
    """
    Remplit la grille d’assemblage aléatoirement avec les types disponibles.
    """
    for i in range(G.size):
        for j in range(G.size):
            if G.core[i][j]:
                t = random.choice(types)
                G.g[i][j] = t.symbole
            else:
                G.g[i][j] = '-'

def afficher_core(G: Grid) -> None:
    """
    Affiche la matrice core de la grille.
    """
    for i in range(G.size):
        print(' '.join('A' if G.core[i][j] else '-' for j in range(G.size)))

def afficher_grille(
    G: Grid,
    palette: List[str],
    couleur_type: Callable[[str], int]
) -> None:
    """
    Affiche la grille d’assemblage.
    """
    for i in range(G.size):
        line = ''
        for j in range(G.size):
            c = G.g[i][j]
            idx = couleur_type(c)
            line += "⬜" if idx == -1 else palette[idx]
        print(line)

def palette() -> List[str]:
    """
    Retourne la palette de symboles pour l'affichage.
    """
    return ["🟩", "🟨", "🟥"]

def couleur_type(symbole: str) -> int:
    """
    Retourne l'index de la couleur associée à un symbole d'assemblage.
    -1 si le symbole n'est pas reconnu (pour afficher ⬜).
    """
    try:
        return palette().index(symbole)
    except ValueError:
        return -1

import math
from typing import List

def symetries_ordre8_circulaire(i: int, j: int, centre: int) -> List[tuple[int, int]]:
    """
    Retourne les coordonnées (x, y) pour les 8 symétries d'un point (i, j) par rapport au centre.
    """
    coords = []
    for dx, dy in [
        ( i,  j), ( j,  i), (-i,  j), (-j,  i),
        ( i, -j), ( j, -i), (-i, -j), (-j, -i)
    ]:
        x, y = centre + dx, centre + dy
        coords.append((x, y))
    return coords

def remplir_grille_symetrique(G: Grid, types: list[TypeAssemblage], nb_types: int) -> None:
    """
    Remplit la grille circulaire par symétrie d'ordre 8, avec pénalité exponentielle sur la réutilisation des types.

    Parameters
    ----------
    G : Grid
        Grille à remplir (objet Grid, déjà généré).
    types : list[TypeAssemblage]
        Liste des types d'assemblage.
    nb_types : int
        Nombre de types d'assemblage.
    """
    size = G.size
    centre = size // 2
    usage = {t.symbole: 0 for t in types}

    # On ne travaille que sur un huitième du disque (i >= 0, j >= i, i <= centre, j <= centre)
    for i in range(centre + 1):
        for j in range(i, centre + 1):
            # Vérifie que la case centrale du huitième est dans le core (cœur nucléaire)
            if not G.core[centre + i][centre + j]:
                continue
            # Choix du type : minimiser la pénalité exponentielle
            best_type = None
            best_score = float('inf')
            for t in types:
                penalty = math.exp(usage[t.symbole])
                if penalty < best_score:
                    best_score = penalty
                    best_type = t
            # Placement symétrique (ordre 8)
            for x, y in symetries_ordre8_circulaire(i, j, centre):
                if 0 <= x < size and 0 <= y < size and G.core[x][y]:
                    G.g[x][y] = best_type.symbole
                    usage[best_type.symbole] += 1
