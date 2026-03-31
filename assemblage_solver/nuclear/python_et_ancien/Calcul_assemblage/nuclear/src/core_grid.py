_ = None

H = 15
L = 15

# Nombre d'assemblages par ligne dans l'octogone
row_lengths = [
    3, 7, 11, 13, 15, 15, 15, 15, 15, 15, 15, 13, 11, 7, 3
]

def build_octagon():
    grid = [[_ for _ in range(L)] for _ in range(H)]
    for i, length in enumerate(row_lengths):
        start = (L - length) // 2
        for j in range(start, start + length):
            grid[i][j] = None  # <-- IMPORTANT : on laisse vide
    return grid

core_shape = build_octagon()

def valid_positions():
    return [(i, j) for i in range(H) for j in range(L) if core_shape[i][j] is not None]
