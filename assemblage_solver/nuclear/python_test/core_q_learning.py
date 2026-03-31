import random
import math
from copy import deepcopy
from .core_config import initial_config, reward
from .core_grid import valid_positions

def random_swap_action():
    positions = valid_positions()
    return random.sample(positions, 2)

def apply_swap_inplace(config, pos1, pos2):
    (i1, j1), (i2, j2) = pos1, pos2
    config[i1][j1], config[i2][j2] = config[i2][j2], config[i1][j1]

def simulated_annealing(episodes=300, steps=40, T_start=1.0, T_end=0.01):
    best_config = None
    best_R = -1e9

    for ep in range(episodes):
        config = initial_config()
        R = reward(config)

        for step in range(steps):
            T = T_start * ((T_end / T_start) ** (step / steps))

            pos1, pos2 = random_swap_action()

            apply_swap_inplace(config, pos1, pos2)
            R_new = reward(config)

            delta = R_new - R

            if delta > 0 or random.random() < math.exp(delta / T):
                R = R_new
            else:
                apply_swap_inplace(config, pos1, pos2)

        if R > best_R:
            best_R = R
            best_config = deepcopy(config)

    return best_config, best_R
