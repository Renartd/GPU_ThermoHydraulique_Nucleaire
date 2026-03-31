import random
import numpy as np
from numba import njit
from .core_grid import core_shape, H, L, valid_positions

TYPES = ['A', 'M', 'N']

POWER = {
    'A': 0.6,
    'M': 1.0,
    'N': 1.4,
}

def initial_config():
    positions = valid_positions()
    n = len(positions)
    per_type = n // 3

    types_list = ['A'] * per_type + ['M'] * per_type + ['N'] * (n - 2 * per_type)
    random.shuffle(types_list)

    config = [[None] * L for _ in range(H)]
    for (i, j), t in zip(positions, types_list):
        config[i][j] = t

    return config

def power_field(config):
    P = np.zeros((H, L), dtype=np.float64)
    for i in range(H):
        for j in range(L):
            t = config[i][j]
            if t is not None:
                P[i][j] = POWER[t]
    return P

@njit
def _reward_from_power_numba(P):
    total = 0.0
    total_sq = 0.0
    count = 0

    for i in range(P.shape[0]):
        for j in range(P.shape[1]):
            v = P[i, j]
            if v > 0:
                total += v
                total_sq += v * v
                count += 1

    if count == 0:
        return 0.0

    mean = total / count
    var = (total_sq / count) - (mean * mean)
    return -var

def reward(config):
    P = power_field(config)
    return _reward_from_power_numba(P)
