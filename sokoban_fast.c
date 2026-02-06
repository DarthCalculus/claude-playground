/*
 * Fast Sokoban solver + optimizer in C.
 * 4x4 grid, BFS solver, hill climbing + random search.
 *
 * State encoding:
 *   Player position: 4 bits (0-15)
 *   Each block position: 4 bits (0-15), or 16 = removed
 *   Hole status: 1 bit each (filled or not)
 *   We pack the state into a 64-bit integer for hashing.
 *
 * Max blocks: 6, Max holes: 4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define GRID_SIZE 4
#define NUM_CELLS 16
#define MAX_BLOCKS 6
#define MAX_HOLES 4

/* Directions: Up, Right, Down, Left */
static const int DR[4] = {-1, 0, 1, 0};
static const int DC[4] = {0, 1, 0, -1};

/* Puzzle definition */
typedef struct {
    uint16_t walls;        /* bitmask of wall positions */
    int exit_pos;          /* 0-15 */
    int player_start;      /* 0-15 */
    int num_blocks;
    int block_pos[MAX_BLOCKS];     /* 0-15 */
    uint8_t block_pushable[MAX_BLOCKS]; /* bitmask: bit d set if pushable from dir d */
    int num_holes;
    int hole_pos[MAX_HOLES];       /* 0-15 */
} Puzzle;

/* BFS state - packed into uint64_t */
/* Layout: player(4 bits) | block0(5 bits) | ... | blockN(5 bits) | hole_mask(4 bits) */
/* 5 bits per block to allow value 16 (removed) */

static inline int pos(int r, int c) { return r * GRID_SIZE + c; }
static inline int row(int p) { return p / GRID_SIZE; }
static inline int col(int p) { return p % GRID_SIZE; }
static inline int in_bounds(int r, int c) { return r >= 0 && r < GRID_SIZE && c >= 0 && c < GRID_SIZE; }

static inline uint64_t pack_state(int player, int *block_pos, int num_blocks, int hole_mask) {
    uint64_t s = player;
    int shift = 4;
    for (int i = 0; i < num_blocks; i++) {
        s |= ((uint64_t)block_pos[i] << shift);
        shift += 5;
    }
    s |= ((uint64_t)hole_mask << shift);
    return s;
}

/* Hash table for visited states */
#define HT_SIZE (1 << 22)  /* 4M entries */
#define HT_MASK (HT_SIZE - 1)

static uint64_t ht_keys[HT_SIZE];
static uint8_t ht_used[HT_SIZE];

static void ht_clear(void) {
    memset(ht_used, 0, sizeof(ht_used));
}

static inline uint64_t hash64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= (x >> 31);
    return x;
}

static inline int ht_insert(uint64_t key) {
    uint64_t h = hash64(key) & HT_MASK;
    for (int i = 0; i < 32; i++) {
        uint64_t idx = (h + i) & HT_MASK;
        if (!ht_used[idx]) {
            ht_used[idx] = 1;
            ht_keys[idx] = key;
            return 1; /* inserted */
        }
        if (ht_keys[idx] == key) {
            return 0; /* already exists */
        }
    }
    return 0; /* table full in this probe range */
}

/* BFS queue */
#define QUEUE_SIZE (1 << 22)
static uint64_t queue_states[QUEUE_SIZE];
static int queue_dist[QUEUE_SIZE];

int solve(const Puzzle *pz) {
    ht_clear();

    int initial_blocks[MAX_BLOCKS];
    for (int i = 0; i < pz->num_blocks; i++)
        initial_blocks[i] = pz->block_pos[i];

    int initial_hole_mask = (1 << pz->num_holes) - 1; /* all holes unfilled */

    uint64_t start = pack_state(pz->player_start, initial_blocks, pz->num_blocks, initial_hole_mask);
    ht_insert(start);

    int qhead = 0, qtail = 0;
    queue_states[qtail] = start;
    queue_dist[qtail] = 0;
    qtail++;

    while (qhead < qtail) {
        uint64_t state = queue_states[qhead];
        int dist = queue_dist[qhead];
        qhead++;

        /* Unpack state */
        int player = state & 0xF;
        int shift = 4;
        int bpos[MAX_BLOCKS];
        for (int i = 0; i < pz->num_blocks; i++) {
            bpos[i] = (state >> shift) & 0x1F;
            shift += 5;
        }
        int hole_mask = (state >> shift) & 0xF;

        int pr = row(player), pc = col(player);

        for (int d = 0; d < 4; d++) {
            int nr = pr + DR[d], nc = pc + DC[d];
            if (!in_bounds(nr, nc)) continue;
            int npos = pos(nr, nc);

            if (pz->walls & (1 << npos)) continue;

            /* Check if it's an unfilled hole */
            int is_unfilled_hole = 0;
            for (int h = 0; h < pz->num_holes; h++) {
                if (pz->hole_pos[h] == npos && (hole_mask & (1 << h))) {
                    is_unfilled_hole = 1;
                    break;
                }
            }
            if (is_unfilled_hole) continue;

            /* Check if there's a block here */
            int block_idx = -1;
            for (int b = 0; b < pz->num_blocks; b++) {
                if (bpos[b] == npos) {
                    block_idx = b;
                    break;
                }
            }

            if (block_idx >= 0) {
                /* Try to push block */
                if (!(pz->block_pushable[block_idx] & (1 << d))) continue;

                int bnr = nr + DR[d], bnc = nc + DC[d];
                if (!in_bounds(bnr, bnc)) continue;
                int bnpos = pos(bnr, bnc);

                if (pz->walls & (1 << bnpos)) continue;

                /* Check if another block is there */
                int blocked = 0;
                for (int b = 0; b < pz->num_blocks; b++) {
                    if (b != block_idx && bpos[b] == bnpos) {
                        blocked = 1;
                        break;
                    }
                }
                if (blocked) continue;

                /* Check if block goes into hole */
                int new_bpos[MAX_BLOCKS];
                memcpy(new_bpos, bpos, sizeof(int) * pz->num_blocks);
                int new_hole_mask = hole_mask;

                int into_hole = 0;
                for (int h = 0; h < pz->num_holes; h++) {
                    if (pz->hole_pos[h] == bnpos && (hole_mask & (1 << h))) {
                        into_hole = 1;
                        new_bpos[block_idx] = 16; /* removed */
                        new_hole_mask &= ~(1 << h);
                        break;
                    }
                }
                if (!into_hole) {
                    new_bpos[block_idx] = bnpos;
                }

                /* Player moves to where block was */
                if (npos == pz->exit_pos) return dist + 1;

                uint64_t new_state = pack_state(npos, new_bpos, pz->num_blocks, new_hole_mask);
                if (ht_insert(new_state)) {
                    if (qtail >= QUEUE_SIZE) return -2; /* queue overflow */
                    queue_states[qtail] = new_state;
                    queue_dist[qtail] = dist + 1;
                    qtail++;
                }
            } else {
                /* Just move */
                if (npos == pz->exit_pos) return dist + 1;

                uint64_t new_state = pack_state(npos, bpos, pz->num_blocks, hole_mask);
                if (ht_insert(new_state)) {
                    if (qtail >= QUEUE_SIZE) return -2; /* queue overflow */
                    queue_states[qtail] = new_state;
                    queue_dist[qtail] = dist + 1;
                    qtail++;
                }
            }
        }
    }

    return -1; /* unsolvable */
}

/* Random number gen (xorshift64) */
static uint64_t rng_state = 12345678901234567ULL;
static inline uint64_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}
static inline int rng_int(int n) { return (int)(rng_next() % n); }

void random_puzzle(Puzzle *pz) {
    /* Shuffle positions */
    int perm[16];
    for (int i = 0; i < 16; i++) perm[i] = i;
    for (int i = 15; i > 0; i--) {
        int j = rng_int(i + 1);
        int tmp = perm[i]; perm[i] = perm[j]; perm[j] = tmp;
    }

    int n_walls = rng_int(9);  /* 0-8 walls */
    int idx = 0;

    pz->walls = 0;
    for (int i = 0; i < n_walls && idx < 16; i++, idx++)
        pz->walls |= (1 << perm[idx]);

    if (16 - idx < 3) { pz->num_blocks = 0; pz->num_holes = 0; pz->exit_pos = 0; pz->player_start = 1; return; }

    pz->exit_pos = perm[idx++];
    pz->player_start = perm[idx++];

    int remaining = 16 - idx;
    pz->num_blocks = rng_int(remaining < 5 ? remaining + 1 : 6);
    if (pz->num_blocks > remaining) pz->num_blocks = remaining;

    for (int i = 0; i < pz->num_blocks; i++) {
        pz->block_pos[i] = perm[idx++];
        pz->block_pushable[i] = 1 + rng_int(15); /* 1-15, non-empty subset */
    }

    remaining = 16 - idx;
    pz->num_holes = rng_int(remaining < 4 ? remaining + 1 : 5);
    if (pz->num_holes > remaining) pz->num_holes = remaining;

    for (int i = 0; i < pz->num_holes; i++)
        pz->hole_pos[i] = perm[idx++];
}

void copy_puzzle(Puzzle *dst, const Puzzle *src) {
    memcpy(dst, src, sizeof(Puzzle));
}

void mutate_puzzle(Puzzle *pz) {
    int mutation = rng_int(12);

    /* Compute occupied mask */
    uint16_t occupied = pz->walls | (1 << pz->exit_pos) | (1 << pz->player_start);
    for (int i = 0; i < pz->num_blocks; i++) occupied |= (1 << pz->block_pos[i]);
    for (int i = 0; i < pz->num_holes; i++) occupied |= (1 << pz->hole_pos[i]);

    int free[16], nfree = 0;
    for (int i = 0; i < 16; i++)
        if (!(occupied & (1 << i))) free[nfree++] = i;

    switch (mutation) {
    case 0: /* move wall */
        if (__builtin_popcount(pz->walls) > 0 && nfree > 0) {
            int walls[16], nw = 0;
            for (int i = 0; i < 16; i++) if (pz->walls & (1 << i)) walls[nw++] = i;
            int w = walls[rng_int(nw)];
            pz->walls &= ~(1 << w);
            pz->walls |= (1 << free[rng_int(nfree)]);
        }
        break;
    case 1: /* add wall */
        if (nfree > 0 && __builtin_popcount(pz->walls) < 10) {
            pz->walls |= (1 << free[rng_int(nfree)]);
        }
        break;
    case 2: /* remove wall */
        if (__builtin_popcount(pz->walls) > 0) {
            int walls[16], nw = 0;
            for (int i = 0; i < 16; i++) if (pz->walls & (1 << i)) walls[nw++] = i;
            pz->walls &= ~(1 << walls[rng_int(nw)]);
        }
        break;
    case 3: /* move exit */
        if (nfree > 0) pz->exit_pos = free[rng_int(nfree)];
        break;
    case 4: /* move player */
        if (nfree > 0) pz->player_start = free[rng_int(nfree)];
        break;
    case 5: /* move block */
        if (pz->num_blocks > 0) {
            /* Recalculate free excluding exit for block placement */
            int bfree[16], nbfree = 0;
            for (int i = 0; i < nfree; i++)
                if (free[i] != pz->exit_pos) bfree[nbfree++] = free[i];
            if (nbfree > 0) {
                int bi = rng_int(pz->num_blocks);
                pz->block_pos[bi] = bfree[rng_int(nbfree)];
            }
        }
        break;
    case 6: /* change block pushable sides */
        if (pz->num_blocks > 0) {
            int bi = rng_int(pz->num_blocks);
            pz->block_pushable[bi] = 1 + rng_int(15);
        }
        break;
    case 7: /* add block */
        if (pz->num_blocks < MAX_BLOCKS) {
            int bfree[16], nbfree = 0;
            for (int i = 0; i < nfree; i++)
                if (free[i] != pz->exit_pos) bfree[nbfree++] = free[i];
            if (nbfree > 0) {
                pz->block_pos[pz->num_blocks] = bfree[rng_int(nbfree)];
                pz->block_pushable[pz->num_blocks] = 1 + rng_int(15);
                pz->num_blocks++;
            }
        }
        break;
    case 8: /* remove block */
        if (pz->num_blocks > 0) {
            int bi = rng_int(pz->num_blocks);
            pz->block_pos[bi] = pz->block_pos[pz->num_blocks - 1];
            pz->block_pushable[bi] = pz->block_pushable[pz->num_blocks - 1];
            pz->num_blocks--;
        }
        break;
    case 9: /* add hole */
        if (pz->num_holes < MAX_HOLES && nfree > 0) {
            pz->hole_pos[pz->num_holes] = free[rng_int(nfree)];
            pz->num_holes++;
        }
        break;
    case 10: /* remove hole */
        if (pz->num_holes > 0) {
            int hi = rng_int(pz->num_holes);
            pz->hole_pos[hi] = pz->hole_pos[pz->num_holes - 1];
            pz->num_holes--;
        }
        break;
    case 11: /* move hole */
        if (pz->num_holes > 0 && nfree > 0) {
            int hi = rng_int(pz->num_holes);
            pz->hole_pos[hi] = free[rng_int(nfree)];
        }
        break;
    }
}

void print_puzzle(const Puzzle *pz, int score) {
    char grid[4][4];
    memset(grid, '.', sizeof(grid));

    for (int i = 0; i < 16; i++)
        if (pz->walls & (1 << i)) grid[row(i)][col(i)] = '#';

    for (int i = 0; i < pz->num_holes; i++)
        grid[row(pz->hole_pos[i])][col(pz->hole_pos[i])] = 'O';

    grid[row(pz->exit_pos)][col(pz->exit_pos)] = 'E';
    grid[row(pz->player_start)][col(pz->player_start)] = '@';

    for (int i = 0; i < pz->num_blocks; i++)
        grid[row(pz->block_pos[i])][col(pz->block_pos[i])] = 'B';

    printf("Grid:\n");
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (c) putchar(' ');
            putchar(grid[r][c]);
        }
        putchar('\n');
    }
    printf("Exit: (%d,%d)\n", row(pz->exit_pos), col(pz->exit_pos));
    printf("Player: (%d,%d)\n", row(pz->player_start), col(pz->player_start));
    for (int i = 0; i < pz->num_blocks; i++) {
        printf("Block %d at (%d,%d): pushable from", i,
               row(pz->block_pos[i]), col(pz->block_pos[i]));
        const char *dirs[] = {"Up", "Right", "Down", "Left"};
        int first = 1;
        for (int d = 0; d < 4; d++) {
            if (pz->block_pushable[i] & (1 << d)) {
                printf("%s %s", first ? "" : ",", dirs[d]);
                first = 0;
            }
        }
        putchar('\n');
    }
    printf("Holes:");
    for (int i = 0; i < pz->num_holes; i++)
        printf(" (%d,%d)", row(pz->hole_pos[i]), col(pz->hole_pos[i]));
    putchar('\n');
    printf("Solution length: %d\n", score);
}

int main(int argc, char **argv) {
    int time_limit = 300;
    if (argc > 1) time_limit = atoi(argv[1]);

    rng_state = (uint64_t)time(NULL) * 6364136223846793005ULL + 1;

    Puzzle best_puzzle;
    int best_score = 0;
    long long evals = 0;

    time_t start_time = time(NULL);

    /* Phase 1: Random search */
    printf("=== Phase 1: Random search ===\n");
    fflush(stdout);

    typedef struct { int score; Puzzle pz; } Seed;
    Seed seeds[200];
    int nseed = 0;

    while (time(NULL) - start_time < time_limit / 5) {
        Puzzle pz;
        random_puzzle(&pz);
        int s = solve(&pz);
        evals++;

        if (s > 0 && nseed < 200) {
            seeds[nseed].score = s;
            seeds[nseed].pz = pz;
            nseed++;
        }

        if (s > best_score) {
            best_score = s;
            best_puzzle = pz;
            printf("[Random] New best: %d (evals=%lld, t=%lds)\n",
                   s, evals, time(NULL) - start_time);
            print_puzzle(&pz, s);
            putchar('\n');
            fflush(stdout);

            if (nseed < 200) {
                seeds[nseed].score = s;
                seeds[nseed].pz = pz;
                nseed++;
            }
        }

        if (evals % 500000 == 0) {
            printf("[Random] evals=%lld, best=%d, t=%lds\n",
                   evals, best_score, time(NULL) - start_time);
            fflush(stdout);
        }
    }

    /* Sort seeds by score (simple bubble sort) */
    for (int i = 0; i < nseed - 1; i++)
        for (int j = i + 1; j < nseed; j++)
            if (seeds[j].score > seeds[i].score) {
                Seed tmp = seeds[i]; seeds[i] = seeds[j]; seeds[j] = tmp;
            }
    if (nseed > 50) nseed = 50;

    printf("\n=== Phase 2: Hill climbing with %d seeds, best=%d ===\n\n", nseed, best_score);
    fflush(stdout);

    /* Phase 2: Hill climbing */
    long long iter = 0;
    while (time(NULL) - start_time < time_limit) {
        Puzzle pz;
        if (nseed > 0 && rng_int(10) < 7) {
            int idx = rng_int(nseed < 10 ? nseed : 10);
            copy_puzzle(&pz, &seeds[idx].pz);
        } else if (best_score > 0) {
            copy_puzzle(&pz, &best_puzzle);
        } else {
            random_puzzle(&pz);
        }

        int nmut = 1 + rng_int(3);
        for (int m = 0; m < nmut; m++)
            mutate_puzzle(&pz);

        int s = solve(&pz);
        evals++;

        if (s > best_score) {
            best_score = s;
            best_puzzle = pz;
            printf("[HillClimb] New best: %d (evals=%lld, t=%lds)\n",
                   s, evals, time(NULL) - start_time);
            print_puzzle(&pz, s);
            putchar('\n');
            fflush(stdout);

            /* Add to seeds */
            if (nseed < 200) {
                seeds[nseed].score = s;
                seeds[nseed].pz = pz;
                nseed++;
            } else {
                seeds[nseed - 1].score = s;
                seeds[nseed - 1].pz = pz;
            }
        } else if (s > 0 && s >= best_score - 3 && nseed < 200) {
            seeds[nseed].score = s;
            seeds[nseed].pz = pz;
            nseed++;
        }

        iter++;
        if (iter % 500000 == 0) {
            printf("[HillClimb] iter=%lld, evals=%lld, best=%d, t=%lds\n",
                   iter, evals, best_score, time(NULL) - start_time);
            fflush(stdout);
        }
    }

    printf("\n==================================================\n");
    printf("FINAL BEST: %d steps (%lld evaluations)\n", best_score, evals);
    printf("==================================================\n");
    if (best_score > 0)
        print_puzzle(&best_puzzle, best_score);

    return 0;
}
