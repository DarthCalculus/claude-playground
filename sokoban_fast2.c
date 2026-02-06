/*
 * Fast Sokoban solver + optimizer in C, v2.
 * Seeded with known best solutions, deeper mutations.
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

static const int DR[4] = {-1, 0, 1, 0};
static const int DC[4] = {0, 1, 0, -1};

typedef struct {
    uint16_t walls;
    int exit_pos;
    int player_start;
    int num_blocks;
    int block_pos[MAX_BLOCKS];
    uint8_t block_pushable[MAX_BLOCKS];
    int num_holes;
    int hole_pos[MAX_HOLES];
} Puzzle;

static inline int pos(int r, int c) { return r * GRID_SIZE + c; }
static inline int row_(int p) { return p / GRID_SIZE; }
static inline int col_(int p) { return p % GRID_SIZE; }
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

#define HT_SIZE (1 << 23)
#define HT_MASK (HT_SIZE - 1)

static uint64_t ht_keys[HT_SIZE];
static uint8_t ht_used[HT_SIZE];

static void ht_clear(void) { memset(ht_used, 0, sizeof(ht_used)); }

static inline uint64_t hash64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= (x >> 31);
    return x;
}

static inline int ht_insert(uint64_t key) {
    uint64_t h = hash64(key) & HT_MASK;
    for (int i = 0; i < 64; i++) {
        uint64_t idx = (h + i) & HT_MASK;
        if (!ht_used[idx]) { ht_used[idx] = 1; ht_keys[idx] = key; return 1; }
        if (ht_keys[idx] == key) return 0;
    }
    return 0;
}

#define QUEUE_SIZE (1 << 23)
static uint64_t queue_states[QUEUE_SIZE];
static int queue_dist[QUEUE_SIZE];

int solve(const Puzzle *pz) {
    ht_clear();
    int initial_blocks[MAX_BLOCKS];
    for (int i = 0; i < pz->num_blocks; i++) initial_blocks[i] = pz->block_pos[i];
    int initial_hole_mask = (1 << pz->num_holes) - 1;
    uint64_t start = pack_state(pz->player_start, initial_blocks, pz->num_blocks, initial_hole_mask);
    ht_insert(start);
    int qhead = 0, qtail = 0;
    queue_states[qtail] = start; queue_dist[qtail] = 0; qtail++;

    while (qhead < qtail) {
        uint64_t state = queue_states[qhead]; int dist = queue_dist[qhead]; qhead++;
        int player = state & 0xF;
        int shift = 4;
        int bpos[MAX_BLOCKS];
        for (int i = 0; i < pz->num_blocks; i++) { bpos[i] = (state >> shift) & 0x1F; shift += 5; }
        int hole_mask = (state >> shift) & 0xF;
        int pr = row_(player), pc = col_(player);

        for (int d = 0; d < 4; d++) {
            int nr = pr + DR[d], nc = pc + DC[d];
            if (!in_bounds(nr, nc)) continue;
            int npos = pos(nr, nc);
            if (pz->walls & (1 << npos)) continue;
            int is_unfilled_hole = 0;
            for (int h = 0; h < pz->num_holes; h++)
                if (pz->hole_pos[h] == npos && (hole_mask & (1 << h))) { is_unfilled_hole = 1; break; }
            if (is_unfilled_hole) continue;

            int block_idx = -1;
            for (int b = 0; b < pz->num_blocks; b++)
                if (bpos[b] == npos) { block_idx = b; break; }

            if (block_idx >= 0) {
                if (!(pz->block_pushable[block_idx] & (1 << d))) continue;
                int bnr = nr + DR[d], bnc = nc + DC[d];
                if (!in_bounds(bnr, bnc)) continue;
                int bnpos = pos(bnr, bnc);
                if (pz->walls & (1 << bnpos)) continue;
                int blocked = 0;
                for (int b = 0; b < pz->num_blocks; b++)
                    if (b != block_idx && bpos[b] == bnpos) { blocked = 1; break; }
                if (blocked) continue;

                int new_bpos[MAX_BLOCKS]; memcpy(new_bpos, bpos, sizeof(int)*pz->num_blocks);
                int new_hole_mask = hole_mask;
                int into_hole = 0;
                for (int h = 0; h < pz->num_holes; h++)
                    if (pz->hole_pos[h] == bnpos && (hole_mask & (1 << h))) {
                        into_hole = 1; new_bpos[block_idx] = 16; new_hole_mask &= ~(1 << h); break;
                    }
                if (!into_hole) new_bpos[block_idx] = bnpos;

                if (npos == pz->exit_pos) return dist + 1;
                uint64_t ns = pack_state(npos, new_bpos, pz->num_blocks, new_hole_mask);
                if (ht_insert(ns)) {
                    if (qtail >= QUEUE_SIZE) return -2;
                    queue_states[qtail] = ns; queue_dist[qtail] = dist + 1; qtail++;
                }
            } else {
                if (npos == pz->exit_pos) return dist + 1;
                uint64_t ns = pack_state(npos, bpos, pz->num_blocks, hole_mask);
                if (ht_insert(ns)) {
                    if (qtail >= QUEUE_SIZE) return -2;
                    queue_states[qtail] = ns; queue_dist[qtail] = dist + 1; qtail++;
                }
            }
        }
    }
    return -1;
}

static uint64_t rng_state;
static inline uint64_t rng_next(void) { rng_state ^= rng_state<<13; rng_state ^= rng_state>>7; rng_state ^= rng_state<<17; return rng_state; }
static inline int rng_int(int n) { return (int)(rng_next() % n); }

void random_puzzle(Puzzle *pz) {
    int perm[16]; for (int i=0;i<16;i++) perm[i]=i;
    for (int i=15;i>0;i--) { int j=rng_int(i+1); int t=perm[i]; perm[i]=perm[j]; perm[j]=t; }
    int n_walls = rng_int(9), idx = 0;
    pz->walls = 0;
    for (int i=0;i<n_walls&&idx<16;i++,idx++) pz->walls |= (1<<perm[idx]);
    if (16-idx<3) { pz->num_blocks=0;pz->num_holes=0;pz->exit_pos=0;pz->player_start=1;return; }
    pz->exit_pos = perm[idx++]; pz->player_start = perm[idx++];
    int remaining = 16-idx;
    pz->num_blocks = rng_int(remaining<6?remaining+1:7);
    if (pz->num_blocks>remaining) pz->num_blocks=remaining;
    for (int i=0;i<pz->num_blocks;i++) { pz->block_pos[i]=perm[idx++]; pz->block_pushable[i]=1+rng_int(15); }
    remaining = 16-idx;
    pz->num_holes = rng_int(remaining<4?remaining+1:5);
    if (pz->num_holes>remaining) pz->num_holes=remaining;
    for (int i=0;i<pz->num_holes;i++) pz->hole_pos[i]=perm[idx++];
}

void mutate_puzzle(Puzzle *pz) {
    uint16_t occupied = pz->walls|(1<<pz->exit_pos)|(1<<pz->player_start);
    for (int i=0;i<pz->num_blocks;i++) occupied|=(1<<pz->block_pos[i]);
    for (int i=0;i<pz->num_holes;i++) occupied|=(1<<pz->hole_pos[i]);
    int free[16],nfree=0;
    for (int i=0;i<16;i++) if (!(occupied&(1<<i))) free[nfree++]=i;

    switch (rng_int(12)) {
    case 0: if (__builtin_popcount(pz->walls)>0&&nfree>0) {
        int w[16],nw=0; for(int i=0;i<16;i++) if(pz->walls&(1<<i)) w[nw++]=i;
        int wi=w[rng_int(nw)]; pz->walls&=~(1<<wi); pz->walls|=(1<<free[rng_int(nfree)]); } break;
    case 1: if (nfree>0&&__builtin_popcount(pz->walls)<10) pz->walls|=(1<<free[rng_int(nfree)]); break;
    case 2: if (__builtin_popcount(pz->walls)>0) { int w[16],nw=0; for(int i=0;i<16;i++) if(pz->walls&(1<<i)) w[nw++]=i; pz->walls&=~(1<<w[rng_int(nw)]); } break;
    case 3: if (nfree>0) pz->exit_pos=free[rng_int(nfree)]; break;
    case 4: if (nfree>0) pz->player_start=free[rng_int(nfree)]; break;
    case 5: if (pz->num_blocks>0) {
        int bf[16],nbf=0; for(int i=0;i<nfree;i++) if(free[i]!=pz->exit_pos) bf[nbf++]=free[i];
        if(nbf>0) { int bi=rng_int(pz->num_blocks); pz->block_pos[bi]=bf[rng_int(nbf)]; } } break;
    case 6: if (pz->num_blocks>0) pz->block_pushable[rng_int(pz->num_blocks)]=1+rng_int(15); break;
    case 7: if (pz->num_blocks<MAX_BLOCKS) {
        int bf[16],nbf=0; for(int i=0;i<nfree;i++) if(free[i]!=pz->exit_pos) bf[nbf++]=free[i];
        if(nbf>0) { pz->block_pos[pz->num_blocks]=bf[rng_int(nbf)]; pz->block_pushable[pz->num_blocks]=1+rng_int(15); pz->num_blocks++; } } break;
    case 8: if (pz->num_blocks>0) { int bi=rng_int(pz->num_blocks); pz->block_pos[bi]=pz->block_pos[pz->num_blocks-1]; pz->block_pushable[bi]=pz->block_pushable[pz->num_blocks-1]; pz->num_blocks--; } break;
    case 9: if (pz->num_holes<MAX_HOLES&&nfree>0) { pz->hole_pos[pz->num_holes]=free[rng_int(nfree)]; pz->num_holes++; } break;
    case 10: if (pz->num_holes>0) { int hi=rng_int(pz->num_holes); pz->hole_pos[hi]=pz->hole_pos[pz->num_holes-1]; pz->num_holes--; } break;
    case 11: if (pz->num_holes>0&&nfree>0) pz->hole_pos[rng_int(pz->num_holes)]=free[rng_int(nfree)]; break;
    }
}

void print_puzzle(const Puzzle *pz, int score) {
    char grid[4][4]; memset(grid,'.', 16);
    for(int i=0;i<16;i++) if(pz->walls&(1<<i)) grid[row_(i)][col_(i)]='#';
    for(int i=0;i<pz->num_holes;i++) grid[row_(pz->hole_pos[i])][col_(pz->hole_pos[i])]='O';
    grid[row_(pz->exit_pos)][col_(pz->exit_pos)]='E';
    grid[row_(pz->player_start)][col_(pz->player_start)]='@';
    for(int i=0;i<pz->num_blocks;i++) grid[row_(pz->block_pos[i])][col_(pz->block_pos[i])]='B';
    printf("Grid:\n");
    for(int r=0;r<4;r++){for(int c=0;c<4;c++){if(c)putchar(' ');putchar(grid[r][c]);}putchar('\n');}
    printf("Exit: (%d,%d)\n",row_(pz->exit_pos),col_(pz->exit_pos));
    printf("Player: (%d,%d)\n",row_(pz->player_start),col_(pz->player_start));
    const char*dirs[]={"Up","Right","Down","Left"};
    for(int i=0;i<pz->num_blocks;i++){
        printf("Block %d at (%d,%d): pushable from",i,row_(pz->block_pos[i]),col_(pz->block_pos[i]));
        int first=1;for(int d=0;d<4;d++)if(pz->block_pushable[i]&(1<<d)){printf("%s %s",first?"":",",dirs[d]);first=0;}
        putchar('\n');
    }
    printf("Holes:");for(int i=0;i<pz->num_holes;i++)printf(" (%d,%d)",row_(pz->hole_pos[i]),col_(pz->hole_pos[i]));
    putchar('\n');
    printf("Solution length: %d\n",score);
}

/* Exhaustive single-neighbor search */
int exhaustive_improve(Puzzle *best_pz, int best_score) {
    Puzzle trial;
    int improved = 0;

    /* Try all pushable side changes for each block */
    for (int b = 0; b < best_pz->num_blocks; b++) {
        uint8_t orig = best_pz->block_pushable[b];
        for (int ps = 1; ps <= 15; ps++) {
            if (ps == orig) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.block_pushable[b] = ps;
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] pushable change: block %d -> %d, score=%d\n", b, ps, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try moving each block to each free position */
    uint16_t occ_base = best_pz->walls | (1 << best_pz->exit_pos) | (1 << best_pz->player_start);
    for (int i = 0; i < best_pz->num_holes; i++) occ_base |= (1 << best_pz->hole_pos[i]);

    for (int b = 0; b < best_pz->num_blocks; b++) {
        uint16_t occ = occ_base;
        for (int i = 0; i < best_pz->num_blocks; i++) if (i != b) occ |= (1 << best_pz->block_pos[i]);
        int orig_pos = best_pz->block_pos[b];

        for (int p = 0; p < 16; p++) {
            if (p == orig_pos || (occ & (1 << p)) || p == best_pz->exit_pos) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.block_pos[b] = p;
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] block %d moved to %d, score=%d\n", b, p, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try moving player */
    {
        uint16_t occ = best_pz->walls | (1 << best_pz->exit_pos);
        for (int i = 0; i < best_pz->num_blocks; i++) occ |= (1 << best_pz->block_pos[i]);
        for (int i = 0; i < best_pz->num_holes; i++) occ |= (1 << best_pz->hole_pos[i]);

        for (int p = 0; p < 16; p++) {
            if (p == best_pz->player_start || (occ & (1 << p))) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.player_start = p;
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] player moved to %d, score=%d\n", p, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try moving exit */
    {
        uint16_t occ = best_pz->walls | (1 << best_pz->player_start);
        for (int i = 0; i < best_pz->num_blocks; i++) occ |= (1 << best_pz->block_pos[i]);
        for (int i = 0; i < best_pz->num_holes; i++) occ |= (1 << best_pz->hole_pos[i]);

        for (int p = 0; p < 16; p++) {
            if (p == best_pz->exit_pos || (occ & (1 << p))) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.exit_pos = p;
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] exit moved to %d, score=%d\n", p, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try moving each hole */
    for (int h = 0; h < best_pz->num_holes; h++) {
        uint16_t occ = best_pz->walls | (1 << best_pz->exit_pos) | (1 << best_pz->player_start);
        for (int i = 0; i < best_pz->num_blocks; i++) occ |= (1 << best_pz->block_pos[i]);
        for (int i = 0; i < best_pz->num_holes; i++) if (i != h) occ |= (1 << best_pz->hole_pos[i]);
        int orig = best_pz->hole_pos[h];

        for (int p = 0; p < 16; p++) {
            if (p == orig || (occ & (1 << p))) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.hole_pos[h] = p;
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] hole %d moved to %d, score=%d\n", h, p, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try adding a wall at each free position */
    {
        uint16_t occ = best_pz->walls | (1 << best_pz->exit_pos) | (1 << best_pz->player_start);
        for (int i = 0; i < best_pz->num_blocks; i++) occ |= (1 << best_pz->block_pos[i]);
        for (int i = 0; i < best_pz->num_holes; i++) occ |= (1 << best_pz->hole_pos[i]);

        for (int p = 0; p < 16; p++) {
            if (occ & (1 << p)) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.walls |= (1 << p);
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] wall added at %d, score=%d\n", p, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try adding a block at each free position with each pushable combo */
    if (best_pz->num_blocks < MAX_BLOCKS) {
        uint16_t occ = best_pz->walls | (1 << best_pz->exit_pos) | (1 << best_pz->player_start);
        for (int i = 0; i < best_pz->num_blocks; i++) occ |= (1 << best_pz->block_pos[i]);
        for (int i = 0; i < best_pz->num_holes; i++) occ |= (1 << best_pz->hole_pos[i]);

        for (int p = 0; p < 16; p++) {
            if ((occ & (1 << p)) || p == best_pz->exit_pos) continue;
            for (int ps = 1; ps <= 15; ps++) {
                memcpy(&trial, best_pz, sizeof(Puzzle));
                trial.block_pos[trial.num_blocks] = p;
                trial.block_pushable[trial.num_blocks] = ps;
                trial.num_blocks++;
                int s = solve(&trial);
                if (s > best_score) {
                    best_score = s;
                    memcpy(best_pz, &trial, sizeof(Puzzle));
                    improved = 1;
                    printf("[Exhaustive] block added at %d ps=%d, score=%d\n", p, ps, s);
                    print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
                }
            }
        }
    }

    /* Try adding a hole at each free position */
    if (best_pz->num_holes < MAX_HOLES) {
        uint16_t occ = best_pz->walls | (1 << best_pz->exit_pos) | (1 << best_pz->player_start);
        for (int i = 0; i < best_pz->num_blocks; i++) occ |= (1 << best_pz->block_pos[i]);
        for (int i = 0; i < best_pz->num_holes; i++) occ |= (1 << best_pz->hole_pos[i]);

        for (int p = 0; p < 16; p++) {
            if (occ & (1 << p)) continue;
            memcpy(&trial, best_pz, sizeof(Puzzle));
            trial.hole_pos[trial.num_holes] = p;
            trial.num_holes++;
            int s = solve(&trial);
            if (s > best_score) {
                best_score = s;
                memcpy(best_pz, &trial, sizeof(Puzzle));
                improved = 1;
                printf("[Exhaustive] hole added at %d, score=%d\n", p, s);
                print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
            }
        }
    }

    /* Try removing each block */
    for (int b = 0; b < best_pz->num_blocks; b++) {
        memcpy(&trial, best_pz, sizeof(Puzzle));
        trial.block_pos[b] = trial.block_pos[trial.num_blocks-1];
        trial.block_pushable[b] = trial.block_pushable[trial.num_blocks-1];
        trial.num_blocks--;
        int s = solve(&trial);
        if (s > best_score) {
            best_score = s;
            memcpy(best_pz, &trial, sizeof(Puzzle));
            improved = 1;
            printf("[Exhaustive] block %d removed, score=%d\n", b, s);
            print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
        }
    }

    /* Try removing each hole */
    for (int h = 0; h < best_pz->num_holes; h++) {
        memcpy(&trial, best_pz, sizeof(Puzzle));
        trial.hole_pos[h] = trial.hole_pos[trial.num_holes-1];
        trial.num_holes--;
        int s = solve(&trial);
        if (s > best_score) {
            best_score = s;
            memcpy(best_pz, &trial, sizeof(Puzzle));
            improved = 1;
            printf("[Exhaustive] hole %d removed, score=%d\n", h, s);
            print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
        }
    }

    /* Try removing each wall */
    for (int w = 0; w < 16; w++) {
        if (!(best_pz->walls & (1 << w))) continue;
        memcpy(&trial, best_pz, sizeof(Puzzle));
        trial.walls &= ~(1 << w);
        int s = solve(&trial);
        if (s > best_score) {
            best_score = s;
            memcpy(best_pz, &trial, sizeof(Puzzle));
            improved = 1;
            printf("[Exhaustive] wall at %d removed, score=%d\n", w, s);
            print_puzzle(best_pz, s); printf("\n"); fflush(stdout);
        }
    }

    return improved ? best_score : -1;
}

int main(int argc, char **argv) {
    int time_limit = 300;
    if (argc > 1) time_limit = atoi(argv[1]);
    rng_state = (uint64_t)time(NULL) * 6364136223846793005ULL + 1442695040888963407ULL;

    /* Seed with known best: 28-step puzzle */
    Puzzle seed28;
    seed28.walls = 0;
    seed28.exit_pos = pos(3,3);
    seed28.player_start = pos(2,2);
    seed28.num_blocks = 5;
    seed28.block_pos[0] = pos(1,3); seed28.block_pushable[0] = 1;    /* Up */
    seed28.block_pos[1] = pos(1,1); seed28.block_pushable[1] = 14;   /* Right|Down|Left */
    seed28.block_pos[2] = pos(2,3); seed28.block_pushable[2] = 7;    /* Up|Right|Down */
    seed28.block_pos[3] = pos(1,2); seed28.block_pushable[3] = 1;    /* Up */
    seed28.block_pos[4] = pos(0,2); seed28.block_pushable[4] = 15;   /* All */
    seed28.num_holes = 2;
    seed28.hole_pos[0] = pos(3,1);
    seed28.hole_pos[1] = pos(3,2);

    int s28 = solve(&seed28);
    printf("Seed 28-step puzzle verifies: %d\n", s28);
    print_puzzle(&seed28, s28);
    printf("\n");

    Puzzle best_puzzle = seed28;
    int best_score = s28;

    /* Exhaustive neighbor search first */
    printf("=== Exhaustive neighbor search ===\n"); fflush(stdout);
    int improved = 1;
    while (improved) {
        int new_score = exhaustive_improve(&best_puzzle, best_score);
        if (new_score > 0) {
            best_score = new_score;
            printf("Exhaustive improved to %d, re-scanning...\n", best_score); fflush(stdout);
        } else {
            improved = 0;
        }
    }
    printf("Exhaustive search done, best=%d\n\n", best_score); fflush(stdout);

    /* Now hill climb with random mutations */
    printf("=== Hill climbing from %d ===\n", best_score); fflush(stdout);
    time_t start = time(NULL);
    long long evals = 0;

    typedef struct { int score; Puzzle pz; } Seed;
    Seed seeds[200];
    int nseed = 0;
    seeds[nseed].score = best_score; seeds[nseed].pz = best_puzzle; nseed++;

    /* Also add random search results */
    printf("Generating random seeds...\n"); fflush(stdout);
    for (long long i = 0; i < 2000000 && time(NULL) - start < time_limit / 4; i++) {
        Puzzle pz; random_puzzle(&pz);
        int s = solve(&pz); evals++;
        if (s > best_score) {
            best_score = s; best_puzzle = pz;
            printf("[Random] New best: %d (evals=%lld)\n", s, evals);
            print_puzzle(&pz, s); printf("\n"); fflush(stdout);
        }
        if (s > 0 && nseed < 200) { seeds[nseed].score = s; seeds[nseed].pz = pz; nseed++; }
        if (evals % 500000 == 0) { printf("[Random] evals=%lld, best=%d\n", evals, best_score); fflush(stdout); }
    }

    /* Sort seeds */
    for (int i=0;i<nseed-1;i++) for(int j=i+1;j<nseed;j++) if(seeds[j].score>seeds[i].score) {
        Seed t=seeds[i]; seeds[i]=seeds[j]; seeds[j]=t;
    }
    if (nseed > 50) nseed = 50;

    printf("\nHill climbing with %d seeds, best=%d\n", nseed, best_score); fflush(stdout);

    long long iter = 0;
    while (time(NULL) - start < time_limit) {
        Puzzle pz;
        if (rng_int(10) < 5) {
            pz = best_puzzle;
        } else if (nseed > 0 && rng_int(10) < 7) {
            pz = seeds[rng_int(nseed < 10 ? nseed : 10)].pz;
        } else {
            random_puzzle(&pz);
        }

        int nmut = 1 + rng_int(4);
        for (int m = 0; m < nmut; m++) mutate_puzzle(&pz);

        int s = solve(&pz); evals++;
        if (s > best_score) {
            best_score = s; best_puzzle = pz;
            printf("[HC] New best: %d (evals=%lld, t=%lds)\n", s, evals, time(NULL)-start);
            print_puzzle(&pz, s); printf("\n"); fflush(stdout);

            /* Run exhaustive on new best */
            improved = 1;
            while (improved) {
                int ns = exhaustive_improve(&best_puzzle, best_score);
                if (ns > 0) { best_score = ns; printf("Exhaustive improved to %d\n", best_score); fflush(stdout); }
                else improved = 0;
            }

            if (nseed < 200) { seeds[nseed].score = best_score; seeds[nseed].pz = best_puzzle; nseed++; }
        } else if (s > 0 && s >= best_score - 5 && nseed < 200) {
            seeds[nseed].score = s; seeds[nseed].pz = pz; nseed++;
        }
        iter++;
        if (iter % 500000 == 0) { printf("[HC] iter=%lld, evals=%lld, best=%d, t=%lds\n", iter, evals, best_score, time(NULL)-start); fflush(stdout); }
    }

    printf("\n==================================================\n");
    printf("FINAL BEST: %d steps (%lld evaluations)\n", best_score, evals);
    printf("==================================================\n");
    print_puzzle(&best_puzzle, best_score);
    return 0;
}
