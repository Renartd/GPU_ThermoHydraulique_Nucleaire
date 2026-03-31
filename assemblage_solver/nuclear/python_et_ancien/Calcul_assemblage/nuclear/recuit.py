from typing import List, Callable
import random
import math
from grid import Grid
from types_assemblage import TypeAssemblage

def recuit_simule(
    G: Grid,
    types: List[TypeAssemblage],
    nb_types: int,
    eval_thermique: Callable[[Grid, List[TypeAssemblage], int], float],
    temperature_init: float = 100.0,
    temperature_min: float = 1.0,
    alpha: float = 0.95,
    max_iter: int = 1000,
    seed: int | None = None
) -> Grid:
    """
    Algorithme de recuit simulé pour optimiser la grille d'assemblage.

    Parameters
    ----------
    G : Grid
        Grille à optimiser (sera copiée).
    types : List[TypeAssemblage]
        Types d'assemblages disponibles.
    nb_types : int
        Nombre de types.
    eval_thermique : Callable
        Fonction d'évaluation thermique (retourne un score à minimiser).
    temperature_init : float, optional
        Température initiale (par défaut 100.0).
    temperature_min : float, optional
        Température minimale (par défaut 1.0).
    alpha : float, optional
        Taux de refroidissement (par défaut 0.95).
    max_iter : int, optional
        Nombre maximal d'itérations (par défaut 1000).
    seed : int or None, optional
        Graine aléatoire pour reproductibilité.

    Returns
    -------
    Grid
        Grille optimisée.

    Raises
    ------
    ValueError
        Si la grille ou les types sont invalides.

    Examples
    --------
    # from grid import generer_grille_circulaire
    # from types_assemblage import definir_types
    # G = generer_grille_circulaire(5)
    # types = definir_types()
    # recuit_simule(G, types, len(types), eval_thermique_gradient)
    """
    if seed is not None:
        random.seed(seed)

    import copy
    G_best = copy.deepcopy(G)
    G_current = copy.deepcopy(G)

    score_best = eval_thermique(G_best, types, nb_types)
    score_current = score_best

    temperature = temperature_init
    n = G.size

    for iteration in range(max_iter):
        # Génère un voisin en échangeant deux assemblages aléatoires
        i1, j1, i2, j2 = _random_core_positions(G_current)
        if (i1, j1) == (i2, j2):
            continue  # Ignore si identiques

        # Swap
        G_neighbor = copy.deepcopy(G_current)
        G_neighbor.g[i1][j1], G_neighbor.g[i2][j2] = G_neighbor.g[i2][j2], G_neighbor.g[i1][j1]

        score_neighbor = eval_thermique(G_neighbor, types, nb_types)

        delta = score_neighbor - score_current

        # Acceptation selon Metropolis
        if delta < 0 or random.random() < math.exp(-delta / temperature):
            G_current = G_neighbor
            score_current = score_neighbor
            if score_neighbor < score_best:
                G_best = copy.deepcopy(G_neighbor)
                score_best = score_neighbor

        # Refroidissement
        temperature *= alpha
        if temperature < temperature_min:
            break

    return G_best

def _random_core_positions(G: Grid) -> tuple[int, int, int, int]:
    """
    Sélectionne deux positions aléatoires dans le cœur de la grille.

    Parameters
    ----------
    G : Grid
        Grille.

    Returns
    -------
    tuple[int, int, int, int]
        Coordonnées (i1, j1, i2, j2).
    """
    positions = [(i, j) for i in range(G.size) for j in range(G.size) if G.core[i][j]]
    if len(positions) < 2:
        return (0, 0, 0, 0)
    (i1, j1), (i2, j2) = random.sample(positions, 2)
    return i1, j1, i2, j2

def eval_thermique_gradient(
    G: Grid,
    types: List[TypeAssemblage],
    nb_types: int
) -> float:
    """
    Fonction d'évaluation thermique : retourne le gradient maximal.

    Parameters
    ----------
    G : Grid
        Grille.
    types : List[TypeAssemblage]
        Types d'assemblages.
    nb_types : int
        Nombre de types.

    Returns
    -------
    float
        Gradient maximal (à minimiser).
    """
    # Calcul de la carte thermique
    Tfield = [[0.0 for _ in range(G.size)] for _ in range(G.size)]
    from thermique import calculer_carte_thermique, diffusion_thermique
    calculer_carte_thermique(G, types, nb_types, Tfield)
    diffusion_thermique(G, Tfield, 40)

    grad_max = 0.0
    for i in range(G.size):
        for j in range(G.size):
            if not G.core[i][j]:
                continue
            for di, dj in [(-1,0), (1,0), (0,-1), (0,1)]:
                ni, nj = i + di, j + dj
                if 0 <= ni < G.size and 0 <= nj < G.size and G.core[ni][nj]:
                    grad = abs(Tfield[i][j] - Tfield[ni][nj])
                    if grad > grad_max:
                        grad_max = grad
    return grad_max
