from typing import List
from types_assemblage import TypeAssemblage

def calculer_carte_thermique(
    G,
    types: List[TypeAssemblage],
    nb_types: int,
    Tfield: List[List[float]]
) -> None:
    """
    Calcule la carte thermique initiale : chaque case reçoit la puissance thermique de son assemblage.

    Parameters
    ----------
    G : Grid
        Grille d’assemblage.
    types : List[TypeAssemblage]
        Liste des types d’assemblages.
    nb_types : int
        Nombre de types.
    Tfield : List[List[float]]
        Matrice à remplir (modifiée en place).

    Returns
    -------
    None

    Notes
    -----
    Tfield est modifiée en place.
    """
    symbole2cond = {t.symbole: t.conductivite for t in types}
    for i in range(G.size):
        for j in range(G.size):
            symbole = G.g[i][j]
            if G.core[i][j] and symbole in symbole2cond:
                Tfield[i][j] = symbole2cond[symbole]
            else:
                Tfield[i][j] = 0.0

def diffusion_thermique(
    G,
    Tfield: List[List[float]],
    iterations: int
) -> None:
    """
    Applique une diffusion thermique sur la carte thermique (moyenne locale).

    Parameters
    ----------
    G : Grid
        Grille d’assemblage.
    Tfield : List[List[float]]
        Carte thermique à modifier.
    iterations : int
        Nombre d’itérations de diffusion.

    Returns
    -------
    None

    Notes
    -----
    Tfield est modifiée en place.
    """
    for _ in range(iterations):
        Tfield_new = [[Tfield[i][j] for j in range(G.size)] for i in range(G.size)]
        for i in range(G.size):
            for j in range(G.size):
                if not G.core[i][j]:
                    continue
                voisins = []
                for di, dj in [(-1,0), (1,0), (0,-1), (0,1)]:
                    ni, nj = i + di, j + dj
                    if 0 <= ni < G.size and 0 <= nj < G.size and G.core[ni][nj]:
                        voisins.append(Tfield[ni][nj])
                if voisins:
                    Tfield_new[i][j] = (Tfield[i][j] + sum(voisins)) / (len(voisins) + 1)
        # Mise à jour de la carte thermique
        for i in range(G.size):
            for j in range(G.size):
                Tfield[i][j] = Tfield_new[i][j]

def evaluer_thermique(
    G,
    Tfield: List[List[float]],
    Tmin: float,
    Tmax: float,
    deltaT: float,
    grad_max: float
) -> None:
    """
    Évalue et affiche les résultats thermiques (min, max, delta, gradient).

    Parameters
    ----------
    G : Grid
        Grille d’assemblage.
    Tfield : List[List[float]]
        Carte thermique.
    Tmin, Tmax, deltaT, grad_max : float
        Variables à remplir (non utilisées ici, calculées localement).

    Returns
    -------
    None

    Notes
    -----
    Affiche les valeurs min, max, delta et gradient maximal.
    """
    valeurs = [Tfield[i][j] for i in range(G.size) for j in range(G.size) if G.core[i][j]]
    if valeurs:
        Tmin = min(valeurs)
        Tmax = max(valeurs)
        deltaT = Tmax - Tmin
        grad_max = 0.0
        # Calcul du gradient maximal (différence entre voisins)
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
        print(f"Tmin = {Tmin:.4f}")
        print(f"Tmax = {Tmax:.4f}")
        print(f"ΔT   = {deltaT:.4f}")
        print(f"Gradient max = {grad_max:.4f}")
    else:
        print("Aucune valeur thermique à évaluer.")
