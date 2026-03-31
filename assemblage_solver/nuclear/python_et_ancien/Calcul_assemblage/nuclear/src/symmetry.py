from .core_grid import H, L, core_shape

def symmetric_positions(i, j):
    return [
        (i, j),
        (i, L - j - 1),
        (H - i - 1, j),
        (H - i - 1, L - j - 1)
    ]

def apply_quadripole_symmetry(config):
    for i in range(H):
        for j in range(L):
            # On ignore les cases hors octogone
            if core_shape[i][j] is None:
                continue

            positions = symmetric_positions(i, j)

            # On récupère uniquement les positions valides
            valid_pos = [(x, y) for (x, y) in positions if core_shape[x][y] is not None]

            # On récupère les valeurs existantes dans ces positions
            values = [config[x][y] for (x, y) in valid_pos]

            if not values:
                continue

            v = values[0]

            # On applique la symétrie uniquement dans les cases valides
            for (x, y) in valid_pos:
                config[x][y] = v

    return config
