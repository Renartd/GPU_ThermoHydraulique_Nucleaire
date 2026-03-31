from typing import List
from grid import Grid

def evaluer(G: Grid, diffusion_thermique) -> None:
    """
    Évalue les températures et gradients sur la grille.

    Parameters
    ----------
    G : Grid
        Grille à évaluer.
    diffusion_thermique : Callable
        Fonction de diffusion thermique.

    Returns
    -------
    None
    """
    n = G.size
    T = [[0.0 for _ in range(n)] for _ in range(n)]
    diffusion_thermique(G, T, 40)

    Tmin = float('inf')
    Tmax = float('-inf')
    gradmax = 0.0

    for i in range(n):
        for j in range(n):
            if not G.core[i][j]:
                continue
            v = T[i][j]
            Tmin = min(Tmin, v)
            Tmax = max(Tmax, v)
            # Calcul du gradient maximal
            if i > 0   and G.core[i-1][j]: gradmax = max(gradmax, abs(v - T[i-1][j]))
            if i < n-1 and G.core[i+1][j]: gradmax = max(gradmax, abs(v - T[i+1][j]))
            if j > 0   and G.core[i][j-1]: gradmax = max(gradmax, abs(v - T[i][j-1]))
            if j < n-1 and G.core[i][j+1]: gradmax = max(gradmax, abs(v - T[i][j+1]))

    print(f"Tmin = {Tmin:.4f}")
    print(f"Tmax = {Tmax:.4f}")
    print(f"ΔT   = {Tmax - Tmin:.4f}")
    print(f"Gradient max = {gradmax:.4f}")
