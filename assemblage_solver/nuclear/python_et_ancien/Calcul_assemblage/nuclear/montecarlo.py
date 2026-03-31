import random
import math
from typing import List, Callable
from grid import Grid
from types_assemblage import TypeAssemblage

def rand01() -> float:
    """
    Retourne un float aléatoire entre 0 et 1.

    Returns
    -------
    float
        Valeur aléatoire dans [0, 1].
    """
    return random.random()

def echanger_assemblages(G: Grid, i1: int, j1: int, i2: int, j2: int) -> None:
    """
    Échange deux assemblages dans la grille.

    Parameters
    ----------
    G : Grid
        Grille d'assemblage.
    i1, j1 : int
        Indices du premier assemblage.
    i2, j2 : int
        Indices du second assemblage.

    Returns
    -------
    None
    """
    tmp = G.g[i1][j1]
    G.g[i1][j1] = G.g[i2][j2]
    G.g[i2][j2] = tmp

def monte_carlo_metropolis(
    G: Grid,
    types: List[TypeAssemblage],
    nb_types: int,
    iterations: int,
    T: float,
    Tfield: List[List[float]],
    calculer_carte_thermique: Callable,
    diffusion_thermique: Callable,
    evaluer_thermique: Callable
) -> None:
    """
    Algorithme de Monte Carlo Metropolis pour optimiser la répartition des assemblages.

    Parameters
    ----------
    G : Grid
        Grille à optimiser.
    types : List[TypeAssemblage]
        Types d’assemblages.
    nb_types : int
        Nombre de types.
    iterations : int
        Nombre d’itérations.
    T : float
        Température.
    Tfield : List[List[float]]
        Champ thermique.
    calculer_carte_thermique : Callable
        Fonction de calcul thermique.
    diffusion_thermique : Callable
        Fonction de diffusion thermique.
    evaluer_thermique : Callable
        Fonction d’évaluation thermique.

    Returns
    -------
    None
    """
    Tmin = Tmax = deltaT = grad_max = 0.0
    calculer_carte_thermique(G, types, nb_types, Tfield)
    diffusion_thermique(G, Tfield, 5)
    evaluer_thermique(G, Tfield, Tmin, Tmax, deltaT, grad_max)

    for it in range(iterations):
        # Sélection aléatoire de deux assemblages dans le cœur
        while True:
            i1 = random.randint(0, G.size - 1)
            j1 = random.randint(0, G.size - 1)
            if G.core[i1][j1]:
                break
        while True:
            i2 = random.randint(0, G.size - 1)
            j2 = random.randint(0, G.size - 1)
            if G.core[i2][j2]:
                break
        echanger_assemblages(G, i1, j1, i2, j2)
        calculer_carte_thermique(G, types, nb_types, Tfield)
        diffusion_thermique(G, Tfield, 5)
        Tmin2 = Tmax2 = deltaT2 = grad2 = 0.0
        evaluer_thermique(G, Tfield, Tmin2, Tmax2, deltaT2, grad2)
        dE = deltaT2 - deltaT
        if dE < 0:
            deltaT = deltaT2
        else:
            p = math.exp(-dE / T)
            r = rand01()
            if r < p:
                deltaT = deltaT2
            else:
                echanger_assemblages(G, i1, j1, i2, j2)  # Annule l'échange
    calculer_carte_thermique(G, types, nb_types, Tfield)
    diffusion_thermique(G, Tfield, 5)
    evaluer_thermique(G, Tfield, Tmin, Tmax, deltaT, grad_max)
    print("Après Monte Carlo :")
    print(f"Tmin = {Tmin:.4f}")
    print(f"Tmax = {Tmax:.4f}")
    print(f"ΔT   = {deltaT:.4f}")
    print(f"Gradient max = {grad_max:.4f}")

def recuit_simule(
    G: Grid,
    types: List[TypeAssemblage],
    nb_types: int,
    iterations: int,
    T0: float,
    alpha: float,
    Tfield: List[List[float]],
    calculer_carte_thermique: Callable,
    diffusion_thermique: Callable,
    evaluer_thermique: Callable
) -> None:
    """
    Algorithme de recuit simulé pour optimiser la répartition des assemblages.

    Parameters
    ----------
    G : Grid
        Grille à optimiser.
    types : List[TypeAssemblage]
        Types d’assemblages.
    nb_types : int
        Nombre de types.
    iterations : int
        Nombre d’itérations.
    T0 : float
        Température initiale.
    alpha : float
        Facteur de refroidissement.
    Tfield : List[List[float]]
        Champ thermique.
    calculer_carte_thermique : Callable
        Fonction de calcul thermique.
    diffusion_thermique : Callable
        Fonction de diffusion thermique.
    evaluer_thermique : Callable
        Fonction d’évaluation thermique.

    Returns
    -------
    None
    """
    Tmin = Tmax = deltaT = grad_max = 0.0
    T = T0
    calculer_carte_thermique(G, types, nb_types, Tfield)
    diffusion_thermique(G, Tfield, 5)
    evaluer_thermique(G, Tfield, Tmin, Tmax, deltaT, grad_max)
    for it in range(iterations):
        while True:
            i1 = random.randint(0, G.size - 1)
            j1 = random.randint(0, G.size - 1)
            if G.core[i1][j1]:
                break
        while True:
            i2 = random.randint(0, G.size - 1)
            j2 = random.randint(0, G.size - 1)
            if G.core[i2][j2]:
                break
        echanger_assemblages(G, i1, j1, i2, j2)
        calculer_carte_thermique(G, types, nb_types, Tfield)
        diffusion_thermique(G, Tfield, 5)
        Tmin2 = Tmax2 = deltaT2 = grad2 = 0.0
        evaluer_thermique(G, Tfield, Tmin2, Tmax2, deltaT2, grad2)
        dE = deltaT2 - deltaT
        if dE < 0:
            deltaT = deltaT2
        else:
            p = math.exp(-dE / T)
            r = rand01()
            if r < p:
                deltaT = deltaT2
            else:
                echanger_assemblages(G, i1, j1, i2, j2)
        T *= alpha
    calculer_carte_thermique(G, types, nb_types, Tfield)
    diffusion_thermique(G, Tfield, 5)
    evaluer_thermique(G, Tfield, Tmin, Tmax, deltaT, grad_max)
    print("Après recuit simulé :")
    print(f"Tmin = {Tmin:.4f}")
    print(f"Tmax = {Tmax:.4f}")
    print(f"ΔT   = {deltaT:.4f}")
    print(f"Gradient max = {grad_max:.4f}")
