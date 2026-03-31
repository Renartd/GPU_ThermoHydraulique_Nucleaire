#include "Symmetry.h"

typedef struct {
    int i;
    int j;
} Position;

static void symmetric_positions(int i, int j, int H, int L,
                                Position out[4], int *count) {
    *count = 0;
    out[(*count)++] = (Position){i, j};
    out[(*count)++] = (Position){i, L - j - 1};
    out[(*count)++] = (Position){H - i - 1, j};
    out[(*count)++] = (Position){H - i - 1, L - j - 1};
}

void apply_quadripole_symmetry(Grid *G) {
    int H = G->size;
    int L = G->size;

    for (int i = 0; i < H; i++) {
        for (int j = 0; j < L; j++) {
            if (!G->core[i][j]) continue;

            Position pos[4];
            int npos = 0;
            symmetric_positions(i, j, H, L, pos, &npos);

            char values[4];
            int vcount = 0;

            for (int k = 0; k < npos; k++) {
                int x = pos[k].i;
                int y = pos[k].j;
                if (G->core[x][y]) {
                    values[vcount++] = G->g[x][y];
                }
            }

            if (vcount == 0) continue;

            char v = values[0];

            for (int k = 0; k < npos; k++) {
                int x = pos[k].i;
                int y = pos[k].j;
                if (G->core[x][y]) {
                    G->g[x][y] = v;
                }
            }
        }
    }
}
