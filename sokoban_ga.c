/*
 * sokoban_ga.c - Genetic Algorithm optimizer for Sokoban max-solution-length.
 *
 * Novel approach over existing code:
 *   - Population-based search (300 individuals) with CROSSOVER
 *   - Combines features from two parent puzzles to escape local optima
 *   - Tournament selection + elitism
 *   - Biased toward 5-6 blocks + 2-4 holes (underexplored region)
 *   - Population diversity tracking to trigger reseeding when stagnant
 *
 * Usage: ./sokoban_ga [time_limit_seconds]  (default: 300)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define GRID_SIZE   4
#define MAX_BLOCKS  6
#define MAX_HOLES   4
#define POP_SIZE    300
#define ELITE_SIZE  30
#define TOURN_SIZE  5

static const int DR[4] = {-1, 0, 1, 0};
static const int DC[4] = {0, 1, 0, -1};

typedef struct {
    uint16_t walls;
    int exit_pos, player_start;
    int num_blocks;
    int      block_pos[MAX_BLOCKS];
    uint8_t  block_pushable[MAX_BLOCKS];
    int num_holes;
    int      hole_pos[MAX_HOLES];
} Puzzle;

typedef struct {
    Puzzle puzzle;
    int    score;
} Individual;

static inline int pos(int r, int c)  { return r * 4 + c; }
static inline int row_(int p)        { return p / 4; }
static inline int col_(int p)        { return p % 4; }
static inline int inb(int r, int c)  { return r >= 0 && r < 4 && c >= 0 && c < 4; }

/* ---- State packing ---- */
static inline uint64_t pack(int pl, int *bp, int nb, int hm) {
    uint64_t s = pl; int sh = 4;
    for (int i = 0; i < nb; i++) { s |= ((uint64_t)bp[i] << sh); sh += 5; }
    s |= ((uint64_t)hm << sh);
    return s;
}

/* ---- BFS solver (identical core to existing C files) ---- */
#define HT_SIZE (1 << 23)
#define HT_MASK (HT_SIZE - 1)
static uint64_t htk[HT_SIZE];
static uint8_t  htu[HT_SIZE];
static void htc(void) { memset(htu, 0, sizeof(htu)); }

static inline uint64_t h64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= (x >> 31); return x;
}
static inline int hti(uint64_t k) {
    uint64_t h = h64(k) & HT_MASK;
    for (int i = 0; i < 64; i++) {
        uint64_t idx = (h + i) & HT_MASK;
        if (!htu[idx]) { htu[idx] = 1; htk[idx] = k; return 1; }
        if (htk[idx] == k) return 0;
    }
    return 0;
}

#define QSZ (1 << 23)
static uint64_t qs[QSZ];
static int      qd[QSZ];

int solve(const Puzzle *pz) {
    htc();
    int ib[MAX_BLOCKS];
    for (int i = 0; i < pz->num_blocks; i++) ib[i] = pz->block_pos[i];
    int ihm = (1 << pz->num_holes) - 1;
    uint64_t st = pack(pz->player_start, ib, pz->num_blocks, ihm);
    hti(st);
    int qh = 0, qt = 0;
    qs[qt] = st; qd[qt] = 0; qt++;

    while (qh < qt) {
        uint64_t s = qs[qh]; int dist = qd[qh]; qh++;
        int pl = s & 0xF, sh = 4, bp[MAX_BLOCKS];
        for (int i = 0; i < pz->num_blocks; i++) { bp[i] = (s >> sh) & 0x1F; sh += 5; }
        int hm = (s >> sh) & 0xF, pr = row_(pl), pc = col_(pl);

        for (int d = 0; d < 4; d++) {
            int nr = pr + DR[d], nc = pc + DC[d];
            if (!inb(nr, nc)) continue;
            int np = pos(nr, nc);
            if (pz->walls & (1 << np)) continue;
            int uh = 0;
            for (int h = 0; h < pz->num_holes; h++)
                if (pz->hole_pos[h] == np && (hm & (1 << h))) { uh = 1; break; }
            if (uh) continue;

            int bi = -1;
            for (int b = 0; b < pz->num_blocks; b++)
                if (bp[b] == np) { bi = b; break; }

            if (bi >= 0) {
                if (!(pz->block_pushable[bi] & (1 << d))) continue;
                int bnr = nr + DR[d], bnc = nc + DC[d];
                if (!inb(bnr, bnc)) continue;
                int bnp = pos(bnr, bnc);
                if (pz->walls & (1 << bnp)) continue;
                int bl = 0;
                for (int b = 0; b < pz->num_blocks; b++)
                    if (b != bi && bp[b] == bnp) { bl = 1; break; }
                if (bl) continue;

                int nb[MAX_BLOCKS]; memcpy(nb, bp, sizeof(int) * pz->num_blocks);
                int nhm = hm, ih = 0;
                for (int h = 0; h < pz->num_holes; h++)
                    if (pz->hole_pos[h] == bnp && (hm & (1 << h))) {
                        ih = 1; nb[bi] = 16; nhm &= ~(1 << h); break;
                    }
                if (!ih) nb[bi] = bnp;
                if (np == pz->exit_pos) return dist + 1;
                uint64_t ns = pack(np, nb, pz->num_blocks, nhm);
                if (hti(ns)) { if (qt >= QSZ) return -2; qs[qt] = ns; qd[qt] = dist + 1; qt++; }
            } else {
                if (np == pz->exit_pos) return dist + 1;
                uint64_t ns = pack(np, bp, pz->num_blocks, hm);
                if (hti(ns)) { if (qt >= QSZ) return -2; qs[qt] = ns; qd[qt] = dist + 1; qt++; }
            }
        }
    }
    return -1;
}

/* ---- RNG ---- */
static uint64_t rng_s;
static inline uint64_t rnx(void) {
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 7; rng_s ^= rng_s << 17; return rng_s;
}
static inline int    ri(int n)  { return (int)(rnx() % (unsigned)n); }
static inline double rf(void)   { return (rnx() & 0xFFFFFFF) / (double)0x10000000; }

/* ---- Random puzzle generation (biased: 5-6 blocks, 2-4 holes) ---- */
static void rand_puzzle(Puzzle *pz) {
    int pm[16];
    for (int i = 0; i < 16; i++) pm[i] = i;
    for (int i = 15; i > 0; i--) { int j = ri(i + 1); int t = pm[i]; pm[i] = pm[j]; pm[j] = t; }

    int nw = ri(4);   /* 0-3 walls */
    int idx = 0;
    pz->walls = 0;
    for (int i = 0; i < nw && idx < 16; i++, idx++) pz->walls |= (1 << pm[idx]);

    if (16 - idx < 3) { pz->num_blocks = 0; pz->num_holes = 0; pz->exit_pos = 0; pz->player_start = 1; return; }

    pz->exit_pos    = pm[idx++];
    pz->player_start = pm[idx++];

    /* Bias toward 5-6 blocks */
    int nb = 4 + ri(3);
    if (nb > 16 - idx) nb = 16 - idx;
    pz->num_blocks = nb;
    for (int i = 0; i < nb; i++) {
        pz->block_pos[i]      = pm[idx++];
        pz->block_pushable[i] = (uint8_t)(1 + ri(15));
    }

    /* Bias toward 2-4 holes */
    int nh = 1 + ri(3);
    if (nh > 16 - idx) nh = 16 - idx;
    if (nh > MAX_HOLES) nh = MAX_HOLES;
    pz->num_holes = nh;
    for (int i = 0; i < nh; i++) pz->hole_pos[i] = pm[idx++];
}

/* ---- Mutation (same repertoire as existing solvers) ---- */
static void mutate(Puzzle *pz) {
    uint16_t occ = pz->walls | (1 << pz->exit_pos) | (1 << pz->player_start);
    for (int i = 0; i < pz->num_blocks; i++) occ |= (1 << pz->block_pos[i]);
    for (int i = 0; i < pz->num_holes; i++)  occ |= (1 << pz->hole_pos[i]);
    int fr[16], nf = 0;
    for (int i = 0; i < 16; i++) if (!(occ & (1 << i))) fr[nf++] = i;

    switch (ri(12)) {
    case 0: /* move wall */
        if (__builtin_popcount(pz->walls) > 0 && nf > 0) {
            int w[16], nw = 0;
            for (int i = 0; i < 16; i++) if (pz->walls & (1 << i)) w[nw++] = i;
            int wi = w[ri(nw)]; pz->walls &= ~(1 << wi); pz->walls |= (1 << fr[ri(nf)]);
        } break;
    case 1: /* add wall */
        if (nf > 0 && __builtin_popcount(pz->walls) < 6)
            pz->walls |= (1 << fr[ri(nf)]); break;
    case 2: /* remove wall */
        if (__builtin_popcount(pz->walls) > 0) {
            int w[16], nw = 0;
            for (int i = 0; i < 16; i++) if (pz->walls & (1 << i)) w[nw++] = i;
            pz->walls &= ~(1 << w[ri(nw)]);
        } break;
    case 3: /* move exit */
        if (nf > 0) pz->exit_pos = fr[ri(nf)]; break;
    case 4: /* move player */
        if (nf > 0) pz->player_start = fr[ri(nf)]; break;
    case 5: /* move a block */
        if (pz->num_blocks > 0) {
            int bf[16], nb = 0;
            for (int i = 0; i < nf; i++) if (fr[i] != pz->exit_pos) bf[nb++] = fr[i];
            if (nb > 0) pz->block_pos[ri(pz->num_blocks)] = bf[ri(nb)];
        } break;
    case 6: /* change block pushable sides */
        if (pz->num_blocks > 0)
            pz->block_pushable[ri(pz->num_blocks)] = (uint8_t)(1 + ri(15)); break;
    case 7: /* add block */
        if (pz->num_blocks < MAX_BLOCKS) {
            int bf[16], nb = 0;
            for (int i = 0; i < nf; i++) if (fr[i] != pz->exit_pos) bf[nb++] = fr[i];
            if (nb > 0) {
                pz->block_pos[pz->num_blocks]      = bf[ri(nb)];
                pz->block_pushable[pz->num_blocks] = (uint8_t)(1 + ri(15));
                pz->num_blocks++;
            }
        } break;
    case 8: /* remove block */
        if (pz->num_blocks > 0) {
            int bi = ri(pz->num_blocks);
            pz->block_pos[bi]      = pz->block_pos[pz->num_blocks - 1];
            pz->block_pushable[bi] = pz->block_pushable[pz->num_blocks - 1];
            pz->num_blocks--;
        } break;
    case 9: /* add hole */
        if (pz->num_holes < MAX_HOLES && nf > 0) {
            pz->hole_pos[pz->num_holes++] = fr[ri(nf)];
        } break;
    case 10: /* remove hole */
        if (pz->num_holes > 0) {
            int hi = ri(pz->num_holes);
            pz->hole_pos[hi] = pz->hole_pos[pz->num_holes - 1];
            pz->num_holes--;
        } break;
    case 11: /* move hole */
        if (pz->num_holes > 0 && nf > 0)
            pz->hole_pos[ri(pz->num_holes)] = fr[ri(nf)]; break;
    }
}

/* ---- Crossover ----
 * Combines features from two parent puzzles.
 * Tracks occupied cells to avoid position conflicts.
 * Block pushable-sides are mixed from both parents when positions overlap.
 */
typedef struct { int p; uint8_t ps; } BEntry;

static Puzzle crossover(const Puzzle *a, const Puzzle *b) {
    Puzzle child;
    uint16_t occ = 0;

    /* Exit: pick from one parent */
    child.exit_pos = (ri(2)) ? a->exit_pos : b->exit_pos;
    occ |= (1 << child.exit_pos);

    /* Player: prefer opposite parent from exit to avoid instant collision */
    int pa_ok = !(a->player_start == child.exit_pos);
    int pb_ok = !(b->player_start == child.exit_pos);
    if (pa_ok && pb_ok) child.player_start = ri(2) ? a->player_start : b->player_start;
    else if (pa_ok)     child.player_start = a->player_start;
    else if (pb_ok)     child.player_start = b->player_start;
    else {
        /* Both conflict — pick any free cell */
        for (int i = 0; i < 16; i++) if (!(occ & (1 << i))) { child.player_start = i; break; }
    }
    occ |= (1 << child.player_start);

    /* Walls: take a cell's wall status from whichever parent we draw from randomly */
    child.walls = 0;
    for (int i = 0; i < 16; i++) {
        if (occ & (1 << i)) continue;
        int wa = (a->walls >> i) & 1;
        int wb = (b->walls >> i) & 1;
        int take = 0;
        if      (wa && wb) take = (ri(4) < 3);   /* both have wall: 75% keep */
        else if (wa || wb) take = (ri(2));         /* one has wall:  50% keep */
        if (take) { child.walls |= (1 << i); occ |= (1 << i); }
    }

    /* Blocks: pool all blocks from both parents, deduplicate by position,
     * mixing pushable-sides when the same position appears in both. */
    BEntry pool[MAX_BLOCKS * 2];
    int    np = 0;

    for (int i = 0; i < a->num_blocks; i++) {
        int p = a->block_pos[i];
        if (occ & (1 << p)) continue;
        pool[np++] = (BEntry){p, a->block_pushable[i]};
    }
    for (int i = 0; i < b->num_blocks; i++) {
        int p = b->block_pos[i];
        if (occ & (1 << p)) continue;
        int found = 0;
        for (int j = 0; j < np; j++) {
            if (pool[j].p == p) {
                /* Same position in both parents: mix pushable sides */
                if (ri(2)) pool[j].ps = b->block_pushable[i];
                else {
                    /* Bitwise blend */
                    uint8_t m = (uint8_t)(rnx() & 0xF);
                    pool[j].ps = (pool[j].ps & m) | (b->block_pushable[i] & ~m);
                    if (!pool[j].ps) pool[j].ps = b->block_pushable[i];
                }
                found = 1; break;
            }
        }
        if (!found && np < MAX_BLOCKS * 2) pool[np++] = (BEntry){p, b->block_pushable[i]};
    }

    /* Shuffle pool then pick target number of blocks */
    for (int i = np - 1; i > 0; i--) {
        int j = ri(i + 1); BEntry t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    int target_nb = 4 + ri(3);
    child.num_blocks = 0;
    for (int i = 0; i < np && child.num_blocks < target_nb && child.num_blocks < MAX_BLOCKS; i++) {
        if (!(occ & (1 << pool[i].p))) {
            child.block_pos[child.num_blocks]      = pool[i].p;
            child.block_pushable[child.num_blocks] = pool[i].ps;
            child.num_blocks++;
            occ |= (1 << pool[i].p);
        }
    }
    /* Fill up with random blocks if we came up short */
    int fr[16], nf = 0;
    for (int i = 0; i < 16; i++) if (!(occ & (1 << i)) && i != child.exit_pos) fr[nf++] = i;
    while (child.num_blocks < 4 && child.num_blocks < MAX_BLOCKS && nf > 0) {
        int pick = ri(nf);
        child.block_pos[child.num_blocks]      = fr[pick];
        child.block_pushable[child.num_blocks] = (uint8_t)(1 + ri(15));
        child.num_blocks++;
        occ |= (1 << fr[pick]);
        fr[pick] = fr[--nf];
    }

    /* Holes: same pool approach */
    int hpool[MAX_HOLES * 2], nhp = 0;
    for (int i = 0; i < a->num_holes; i++) {
        int p = a->hole_pos[i];
        if (!(occ & (1 << p))) hpool[nhp++] = p;
    }
    for (int i = 0; i < b->num_holes; i++) {
        int p = b->hole_pos[i];
        if (occ & (1 << p)) continue;
        int dup = 0;
        for (int j = 0; j < nhp; j++) if (hpool[j] == p) { dup = 1; break; }
        if (!dup && nhp < MAX_HOLES * 2) hpool[nhp++] = p;
    }
    for (int i = nhp - 1; i > 0; i--) { int j = ri(i + 1); int t = hpool[i]; hpool[i] = hpool[j]; hpool[j] = t; }
    int target_nh = 1 + ri(3);
    child.num_holes = 0;
    for (int i = 0; i < nhp && child.num_holes < target_nh && child.num_holes < MAX_HOLES; i++) {
        if (!(occ & (1 << hpool[i]))) {
            child.hole_pos[child.num_holes++] = hpool[i];
            occ |= (1 << hpool[i]);
        }
    }

    return child;
}

/* ---- Tournament selection ---- */
static int tournament(Individual *pop, int n) {
    int best = ri(n);
    for (int i = 1; i < TOURN_SIZE; i++) {
        int c = ri(n);
        if (pop[c].score > pop[best].score) best = c;
    }
    return best;
}

static int cmp_ind(const void *a, const void *b) {
    return ((Individual *)b)->score - ((Individual *)a)->score;
}

static double pop_avg(Individual *pop, int n) {
    double s = 0;
    for (int i = 0; i < n; i++) s += pop[i].score;
    return s / n;
}

/* ---- Print puzzle ---- */
static void ppz(const Puzzle *pz, int sc) {
    char g[4][4]; memset(g, '.', 16);
    for (int i = 0; i < 16; i++) if (pz->walls & (1 << i)) g[row_(i)][col_(i)] = '#';
    for (int i = 0; i < pz->num_holes; i++) g[row_(pz->hole_pos[i])][col_(pz->hole_pos[i])] = 'O';
    g[row_(pz->exit_pos)][col_(pz->exit_pos)] = 'E';
    g[row_(pz->player_start)][col_(pz->player_start)] = '@';
    for (int i = 0; i < pz->num_blocks; i++) g[row_(pz->block_pos[i])][col_(pz->block_pos[i])] = 'B';
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) { if (c) putchar(' '); putchar(g[r][c]); } putchar('\n');
    }
    const char *dn[] = {"U","R","D","L"};
    printf("Exit:(%d,%d) Player:(%d,%d)\n", row_(pz->exit_pos), col_(pz->exit_pos),
           row_(pz->player_start), col_(pz->player_start));
    for (int i = 0; i < pz->num_blocks; i++) {
        printf("B%d@(%d,%d):", i, row_(pz->block_pos[i]), col_(pz->block_pos[i]));
        for (int d = 0; d < 4; d++) if (pz->block_pushable[i] & (1 << d)) printf("%s", dn[d]);
        printf(" ");
    }
    printf("\nHoles:");
    for (int i = 0; i < pz->num_holes; i++) printf("(%d,%d)", row_(pz->hole_pos[i]), col_(pz->hole_pos[i]));
    printf(" Score:%d\n\n", sc);
}

/* ---- Known best puzzle (28 steps) ---- */
static Puzzle known_best(void) {
    Puzzle p;
    p.walls = 0; p.exit_pos = pos(3,3); p.player_start = pos(2,2);
    p.num_blocks = 5;
    p.block_pos[0]=pos(1,3); p.block_pushable[0]=1;
    p.block_pos[1]=pos(1,1); p.block_pushable[1]=14;
    p.block_pos[2]=pos(2,3); p.block_pushable[2]=7;
    p.block_pos[3]=pos(1,2); p.block_pushable[3]=1;
    p.block_pos[4]=pos(0,2); p.block_pushable[4]=15;
    p.num_holes=2; p.hole_pos[0]=pos(3,1); p.hole_pos[1]=pos(3,2);
    return p;
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc, char **argv) {
    int time_limit = 300;
    if (argc > 1) time_limit = atoi(argv[1]);

    rng_s = (uint64_t)time(NULL) * 6364136223846793005ULL + 1442695040888963407ULL;
    time_t t0 = time(NULL);

    printf("=== Sokoban Genetic Algorithm ===\n");
    printf("Pop: %d  Elite: %d  Tournament: %d  Time: %ds\n\n",
           POP_SIZE, ELITE_SIZE, TOURN_SIZE, time_limit);
    fflush(stdout);

    /* Allocate two population buffers */
    static Individual pop[POP_SIZE];
    static Individual next[POP_SIZE];

    /* Global best */
    int    gbest = 28;
    Puzzle gbest_pz = known_best();
    long long evals = 0;

    /* ----- Initialise population ----- */
    pop[0].puzzle = gbest_pz;
    pop[0].score  = gbest;
    for (int i = 1; i < POP_SIZE; i++) {
        /* Seed ~30% from mutations of the known best, rest random */
        if (i < POP_SIZE / 3) {
            pop[i].puzzle = gbest_pz;
            int nm = 1 + ri(5);
            for (int m = 0; m < nm; m++) mutate(&pop[i].puzzle);
        } else {
            rand_puzzle(&pop[i].puzzle);
        }
        int s = solve(&pop[i].puzzle);
        pop[i].score = (s > 0) ? s : 0;
        evals++;
    }
    qsort(pop, POP_SIZE, sizeof(Individual), cmp_ind);
    printf("Initial pop best=%d avg=%.1f\n", pop[0].score, pop_avg(pop, POP_SIZE));
    fflush(stdout);

    /* ----- Generational GA loop ----- */
    int  gen = 0;
    int  stagnant_gens = 0;
    int  last_improvement = 0;

    while (time(NULL) - t0 < time_limit) {
        gen++;

        /* Elites survive unchanged */
        for (int i = 0; i < ELITE_SIZE; i++) next[i] = pop[i];

        /* Generate offspring */
        for (int i = ELITE_SIZE; i < POP_SIZE; i++) {
            Puzzle child;
            int mode = ri(10);

            if (mode < 5) {
                /* Crossover: two tournament winners */
                int ia = tournament(pop, POP_SIZE);
                int ib = tournament(pop, POP_SIZE);
                child = crossover(&pop[ia].puzzle, &pop[ib].puzzle);
                int nm = 1 + ri(3);
                for (int m = 0; m < nm; m++) mutate(&child);
            } else if (mode < 8) {
                /* Mutate an elite */
                int ia = ri(ELITE_SIZE);
                child = pop[ia].puzzle;
                int nm = 1 + ri(4);
                for (int m = 0; m < nm; m++) mutate(&child);
            } else {
                /* Fresh random (maintains diversity) */
                rand_puzzle(&child);
            }

            int s = solve(&child);
            evals++;
            next[i].puzzle = child;
            next[i].score  = (s > 0) ? s : 0;

            if (s > gbest) {
                gbest    = s;
                gbest_pz = child;
                printf("[Gen %d | ev=%lld | t=%lds] *** NEW BEST: %d ***\n",
                       gen, evals, (long)(time(NULL) - t0), s);
                ppz(&child, s);
                fflush(stdout);
                stagnant_gens    = 0;
                last_improvement = gen;
            }
        }

        qsort(next, POP_SIZE, sizeof(Individual), cmp_ind);
        memcpy(pop, next, sizeof(Individual) * POP_SIZE);

        /* Stagnation detection: if no improvement in 500 gens, reseed bottom half */
        if (pop[0].score <= gbest) stagnant_gens++;
        else stagnant_gens = 0;

        if (stagnant_gens > 500) {
            printf("[Gen %d] Stagnant — reseeding bottom half of population\n", gen);
            fflush(stdout);
            for (int i = POP_SIZE / 2; i < POP_SIZE; i++) {
                if (ri(3) == 0) {
                    pop[i].puzzle = gbest_pz;
                    int nm = 2 + ri(6);
                    for (int m = 0; m < nm; m++) mutate(&pop[i].puzzle);
                } else {
                    rand_puzzle(&pop[i].puzzle);
                }
                int s = solve(&pop[i].puzzle);
                pop[i].score = (s > 0) ? s : 0;
                evals++;
            }
            qsort(pop, POP_SIZE, sizeof(Individual), cmp_ind);
            stagnant_gens = 0;
        }

        if (gen % 200 == 0) {
            printf("[Gen %d | ev=%lld | t=%lds] best=%d avg=%.1f global=%d (last imp: gen %d)\n",
                   gen, evals, (long)(time(NULL) - t0),
                   pop[0].score, pop_avg(pop, POP_SIZE), gbest, last_improvement);
            fflush(stdout);
        }
    }

    printf("\n========================================\n");
    printf("FINAL: %d steps (%lld evals, %d generations)\n", gbest, evals, gen);
    printf("========================================\n");
    ppz(&gbest_pz, gbest);
    return 0;
}
