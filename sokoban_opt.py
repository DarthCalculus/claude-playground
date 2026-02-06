"""
Optimized Sokoban puzzle solver with hill climbing.
Uses the core solver but adds mutation-based optimization.
"""

from collections import deque
from itertools import combinations, product
import random
import time
import sys
import copy

GRID_SIZE = 4
DIRS = [(-1, 0), (0, 1), (1, 0), (0, -1)]

def in_bounds(r, c):
    return 0 <= r < GRID_SIZE and 0 <= c < GRID_SIZE

def solve_puzzle(walls, holes, exit_pos, player_start, blocks):
    """BFS solver. Returns shortest solution length or -1."""
    initial_blocks = frozenset((r, c, ps) for r, c, ps in blocks)
    initial_holes = holes
    start_state = (player_start[0], player_start[1], initial_blocks, initial_holes)

    visited = {start_state}
    queue = deque([(start_state, 0)])

    while queue:
        state, dist = queue.popleft()
        pr, pc, block_set, hole_set = state

        block_pos_map = {}
        for br, bc, bps in block_set:
            block_pos_map[(br, bc)] = bps

        for d in range(4):
            dr, dc = DIRS[d]
            nr, nc = pr + dr, pc + dc

            if not in_bounds(nr, nc) or (nr, nc) in walls or (nr, nc) in hole_set:
                continue

            if (nr, nc) in block_pos_map:
                bps = block_pos_map[(nr, nc)]
                if d not in bps:
                    continue
                bnr, bnc = nr + dr, nc + dc
                if not in_bounds(bnr, bnc) or (bnr, bnc) in walls or (bnr, bnc) in block_pos_map:
                    continue

                if (bnr, bnc) in hole_set:
                    new_blocks = frozenset(b for b in block_set if (b[0], b[1]) != (nr, nc))
                    new_holes = hole_set - {(bnr, bnc)}
                else:
                    new_blocks = frozenset(
                        (bnr, bnc, b[2]) if (b[0], b[1]) == (nr, nc) else b
                        for b in block_set
                    )
                    new_holes = hole_set

                new_state = (nr, nc, new_blocks, new_holes)
                if (nr, nc) == exit_pos:
                    return dist + 1
                if new_state not in visited:
                    visited.add(new_state)
                    queue.append((new_state, dist + 1))
            else:
                if (nr, nc) == exit_pos:
                    return dist + 1
                new_state = (nr, nc, block_set, hole_set)
                if new_state not in visited:
                    visited.add(new_state)
                    queue.append((new_state, dist + 1))

    return -1


ALL_POSITIONS = [(r, c) for r in range(GRID_SIZE) for c in range(GRID_SIZE)]
ALL_PUSHABLE = []
for r in range(1, 5):
    for combo in combinations([0,1,2,3], r):
        ALL_PUSHABLE.append(frozenset(combo))


def random_puzzle():
    """Generate a random puzzle."""
    positions = list(ALL_POSITIONS)
    random.shuffle(positions)

    n_walls = random.randint(0, 8)
    walls = frozenset(positions[:n_walls])
    remaining = positions[n_walls:]

    if len(remaining) < 3:
        return None

    exit_pos = remaining[0]
    player_start = remaining[1]

    n_blocks = random.randint(0, min(4, len(remaining) - 2))
    block_positions = remaining[2:2 + n_blocks]
    remaining2 = remaining[2 + n_blocks:]

    n_holes = random.randint(0, min(3, len(remaining2)))
    holes = frozenset(remaining2[:n_holes])

    blocks = tuple((r, c, random.choice(ALL_PUSHABLE)) for r, c in block_positions)

    return {
        'walls': walls,
        'holes': holes,
        'exit': exit_pos,
        'player': player_start,
        'blocks': blocks,
    }


def puzzle_to_tuple(p):
    """Convert puzzle to hashable tuple for dedup."""
    return (p['walls'], p['holes'], p['exit'], p['player'], p['blocks'])


def evaluate(p):
    """Evaluate a puzzle, return solution length."""
    return solve_puzzle(p['walls'], p['holes'], p['exit'], p['player'], p['blocks'])


def mutate(p):
    """Mutate a puzzle slightly."""
    p = {
        'walls': set(p['walls']),
        'holes': set(p['holes']),
        'exit': p['exit'],
        'player': p['player'],
        'blocks': list(p['blocks']),
    }

    def occupied():
        s = set(p['walls']) | set(p['holes']) | {p['exit'], p['player']}
        for b in p['blocks']:
            s.add((b[0], b[1]))
        return s

    def free_positions():
        occ = occupied()
        return [pos for pos in ALL_POSITIONS if pos not in occ]

    mutation = random.choice(['move_wall', 'add_wall', 'remove_wall',
                               'move_exit', 'move_player',
                               'move_block', 'change_block_sides',
                               'add_block', 'remove_block',
                               'add_hole', 'remove_hole', 'move_hole'])

    if mutation == 'move_wall' and p['walls']:
        wall = random.choice(list(p['walls']))
        free = free_positions()
        if free:
            p['walls'].discard(wall)
            p['walls'].add(random.choice(free))
    elif mutation == 'add_wall':
        free = free_positions()
        if free and len(p['walls']) < 10:
            p['walls'].add(random.choice(free))
    elif mutation == 'remove_wall' and p['walls']:
        p['walls'].discard(random.choice(list(p['walls'])))
    elif mutation == 'move_exit':
        free = free_positions()
        if free:
            old = p['exit']
            p['exit'] = random.choice(free)
    elif mutation == 'move_player':
        free = free_positions()
        if free:
            p['player'] = random.choice(free)
    elif mutation == 'move_block' and p['blocks']:
        idx = random.randint(0, len(p['blocks']) - 1)
        free = free_positions()
        # Don't place block on exit
        free = [pos for pos in free if pos != p['exit']]
        if free:
            pos = random.choice(free)
            p['blocks'][idx] = (pos[0], pos[1], p['blocks'][idx][2])
    elif mutation == 'change_block_sides' and p['blocks']:
        idx = random.randint(0, len(p['blocks']) - 1)
        p['blocks'][idx] = (p['blocks'][idx][0], p['blocks'][idx][1], random.choice(ALL_PUSHABLE))
    elif mutation == 'add_block':
        free = free_positions()
        free = [pos for pos in free if pos != p['exit']]
        if free and len(p['blocks']) < 5:
            pos = random.choice(free)
            p['blocks'].append((pos[0], pos[1], random.choice(ALL_PUSHABLE)))
    elif mutation == 'remove_block' and p['blocks']:
        idx = random.randint(0, len(p['blocks']) - 1)
        p['blocks'].pop(idx)
    elif mutation == 'add_hole':
        free = free_positions()
        if free and len(p['holes']) < 4:
            p['holes'].add(random.choice(free))
    elif mutation == 'remove_hole' and p['holes']:
        p['holes'].discard(random.choice(list(p['holes'])))
    elif mutation == 'move_hole' and p['holes']:
        hole = random.choice(list(p['holes']))
        free = free_positions()
        if free:
            p['holes'].discard(hole)
            p['holes'].add(random.choice(free))

    return {
        'walls': frozenset(p['walls']),
        'holes': frozenset(p['holes']),
        'exit': p['exit'],
        'player': p['player'],
        'blocks': tuple(p['blocks']),
    }


def print_puzzle(puzzle, score=None):
    grid = [['.' for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]
    for r, c in puzzle['walls']:
        grid[r][c] = '#'
    for r, c in puzzle['holes']:
        grid[r][c] = 'O'
    er, ec = puzzle['exit']
    grid[er][ec] = 'E'
    pr, pc = puzzle['player']
    grid[pr][pc] = '@'
    for i, (br, bc, ps) in enumerate(puzzle['blocks']):
        grid[br][bc] = 'B'

    print("Grid:")
    for row in grid:
        print(' '.join(row))
    print(f"Exit: {puzzle['exit']}")
    print(f"Player: {puzzle['player']}")
    for i, (br, bc, ps) in enumerate(puzzle['blocks']):
        sides = []
        if 0 in ps: sides.append('Up')
        if 1 in ps: sides.append('Right')
        if 2 in ps: sides.append('Down')
        if 3 in ps: sides.append('Left')
        print(f"Block {i} at ({br},{bc}): pushable from {', '.join(sides)}")
    print(f"Holes: {list(puzzle['holes'])}")
    if score is not None:
        print(f"Solution length: {score}")


def hill_climb(time_limit=300, verbose=True):
    """Combined random search + hill climbing."""
    best_score = 0
    best_puzzle = None
    evals = 0
    start = time.time()

    # Phase 1: Random search to find seeds
    seeds = []
    while time.time() - start < min(30, time_limit * 0.2):
        p = random_puzzle()
        if p is None:
            continue
        score = evaluate(p)
        evals += 1
        if score > 0:
            seeds.append((score, p))
        if score > best_score:
            best_score = score
            best_puzzle = p
            if verbose:
                print(f"[Random] New best: {score} (evals={evals}, t={time.time()-start:.1f}s)")
                print_puzzle(p, score)
                print()
        if evals % 10000 == 0 and verbose:
            print(f"[Random] evals={evals}, best={best_score}, t={time.time()-start:.1f}s")

    # Sort seeds by score
    seeds.sort(key=lambda x: -x[0])
    top_seeds = [p for s, p in seeds[:50]]

    if verbose:
        print(f"\n=== Hill climbing with {len(top_seeds)} seeds, best so far: {best_score} ===\n")

    # Phase 2: Hill climbing from top seeds
    iteration = 0
    while time.time() - start < time_limit:
        # Pick a seed (bias toward better ones)
        if top_seeds and random.random() < 0.7:
            base = random.choice(top_seeds[:10]) if len(top_seeds) >= 10 else random.choice(top_seeds)
        elif best_puzzle:
            base = best_puzzle
        else:
            base = random_puzzle()
            if base is None:
                continue

        # Apply 1-3 mutations
        p = base
        for _ in range(random.randint(1, 3)):
            p = mutate(p)

        score = evaluate(p)
        evals += 1

        if score > best_score:
            best_score = score
            best_puzzle = p
            top_seeds.insert(0, p)
            if verbose:
                print(f"[HillClimb] New best: {score} (evals={evals}, t={time.time()-start:.1f}s)")
                print_puzzle(p, score)
                print()
        elif score > 0 and score >= best_score - 3:
            top_seeds.append(p)
            if len(top_seeds) > 100:
                top_seeds = top_seeds[:50]

        iteration += 1
        if iteration % 10000 == 0 and verbose:
            print(f"[HillClimb] iter={iteration}, evals={evals}, best={best_score}, t={time.time()-start:.1f}s")

    return best_puzzle, best_score, evals


if __name__ == '__main__':
    tl = int(sys.argv[1]) if len(sys.argv) > 1 else 180
    puzzle, score, evals = hill_climb(time_limit=tl, verbose=True)
    print(f"\n{'='*50}")
    print(f"FINAL BEST: {score} steps ({evals} evaluations)")
    print('='*50)
    if puzzle:
        print_puzzle(puzzle, score)
