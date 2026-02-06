"""
Deep local search around known best solutions.
Exhaustively tries all single-mutation neighbors of the best puzzle,
then hill climbs from any improvements.
"""

from sokoban_opt import solve_puzzle, ALL_POSITIONS, ALL_PUSHABLE, print_puzzle, mutate, evaluate
from itertools import combinations
import random
import time

GRID_SIZE = 4

def get_occupied(puzzle):
    occ = set(puzzle['walls']) | set(puzzle['holes']) | {puzzle['exit'], puzzle['player']}
    for b in puzzle['blocks']:
        occ.add((b[0], b[1]))
    return occ

def free_positions(puzzle):
    return [p for p in ALL_POSITIONS if p not in get_occupied(puzzle)]

def exhaustive_neighbors(puzzle):
    """Generate all single-mutation neighbors."""
    neighbors = []
    occ = get_occupied(puzzle)
    free = free_positions(puzzle)
    free_no_exit = [p for p in free if p != puzzle['exit']]

    walls = list(puzzle['walls'])
    holes = list(puzzle['holes'])
    blocks = list(puzzle['blocks'])

    # Move each wall to each free position
    for i, w in enumerate(walls):
        for fp in free:
            new_walls = frozenset(walls[:i] + walls[i+1:] + [fp])
            neighbors.append({
                'walls': new_walls, 'holes': puzzle['holes'],
                'exit': puzzle['exit'], 'player': puzzle['player'],
                'blocks': puzzle['blocks']
            })

    # Remove each wall
    for i, w in enumerate(walls):
        new_walls = frozenset(walls[:i] + walls[i+1:])
        neighbors.append({
            'walls': new_walls, 'holes': puzzle['holes'],
            'exit': puzzle['exit'], 'player': puzzle['player'],
            'blocks': puzzle['blocks']
        })

    # Add a wall at each free position
    for fp in free:
        new_walls = frozenset(list(puzzle['walls']) + [fp])
        neighbors.append({
            'walls': new_walls, 'holes': puzzle['holes'],
            'exit': puzzle['exit'], 'player': puzzle['player'],
            'blocks': puzzle['blocks']
        })

    # Move exit to each free position
    for fp in free:
        neighbors.append({
            'walls': puzzle['walls'], 'holes': puzzle['holes'],
            'exit': fp, 'player': puzzle['player'],
            'blocks': puzzle['blocks']
        })

    # Move player to each free position
    for fp in free:
        neighbors.append({
            'walls': puzzle['walls'], 'holes': puzzle['holes'],
            'exit': puzzle['exit'], 'player': fp,
            'blocks': puzzle['blocks']
        })

    # For each block: move to each free position, change pushable sides
    for i, (br, bc, bps) in enumerate(blocks):
        # Move block
        for fp in free_no_exit:
            new_blocks = list(blocks)
            new_blocks[i] = (fp[0], fp[1], bps)
            neighbors.append({
                'walls': puzzle['walls'], 'holes': puzzle['holes'],
                'exit': puzzle['exit'], 'player': puzzle['player'],
                'blocks': tuple(new_blocks)
            })
        # Change pushable sides
        for ps in ALL_PUSHABLE:
            if ps != bps:
                new_blocks = list(blocks)
                new_blocks[i] = (br, bc, ps)
                neighbors.append({
                    'walls': puzzle['walls'], 'holes': puzzle['holes'],
                    'exit': puzzle['exit'], 'player': puzzle['player'],
                    'blocks': tuple(new_blocks)
                })

    # Remove each block
    for i in range(len(blocks)):
        new_blocks = tuple(blocks[:i] + blocks[i+1:])
        neighbors.append({
            'walls': puzzle['walls'], 'holes': puzzle['holes'],
            'exit': puzzle['exit'], 'player': puzzle['player'],
            'blocks': new_blocks
        })

    # Add a block at each free position with each pushable side combo
    for fp in free_no_exit:
        for ps in ALL_PUSHABLE:
            new_blocks = tuple(list(blocks) + [(fp[0], fp[1], ps)])
            neighbors.append({
                'walls': puzzle['walls'], 'holes': puzzle['holes'],
                'exit': puzzle['exit'], 'player': puzzle['player'],
                'blocks': new_blocks
            })

    # Move each hole
    for i, h in enumerate(holes):
        for fp in free:
            new_holes = frozenset(holes[:i] + holes[i+1:] + [fp])
            neighbors.append({
                'walls': puzzle['walls'], 'holes': new_holes,
                'exit': puzzle['exit'], 'player': puzzle['player'],
                'blocks': puzzle['blocks']
            })

    # Remove each hole
    for i in range(len(holes)):
        new_holes = frozenset(holes[:i] + holes[i+1:])
        neighbors.append({
            'walls': puzzle['walls'], 'holes': new_holes,
            'exit': puzzle['exit'], 'player': puzzle['player'],
            'blocks': puzzle['blocks']
        })

    # Add a hole
    for fp in free:
        new_holes = frozenset(list(puzzle['holes']) + [fp])
        neighbors.append({
            'walls': puzzle['walls'], 'holes': new_holes,
            'exit': puzzle['exit'], 'player': puzzle['player'],
            'blocks': puzzle['blocks']
        })

    return neighbors


# Known best puzzles
BEST_28 = {
    'walls': frozenset(),
    'holes': frozenset([(3,1), (3,2)]),
    'exit': (3,3),
    'player': (2,2),
    'blocks': (
        (1, 3, frozenset({0})),
        (1, 1, frozenset({1,2,3})),
        (2, 3, frozenset({0,1,2})),
        (1, 2, frozenset({0})),
        (0, 2, frozenset({0,1,2,3})),
    ),
}

BEST_27_C = {
    'walls': frozenset(),
    'holes': frozenset([(1,0), (1,1), (2,0), (2,3)]),
    'exit': (0,0),
    'player': (1,2),
    'blocks': (
        (2, 1, frozenset({1,3})),     # Right, Left
        (0, 1, frozenset({1,2})),     # Right, Down
        (2, 2, frozenset({0,2,3})),   # Up, Down, Left
        (3, 1, frozenset({0,1})),     # Up, Right
        (1, 3, frozenset({1,2})),     # Right, Down
    ),
}


def deep_search(start_puzzle, start_score, time_limit=600):
    best = start_puzzle
    best_score = start_score
    improved = True
    total_evals = 0
    start_time = time.time()

    while improved and time.time() - start_time < time_limit:
        improved = False
        neighbors = exhaustive_neighbors(best)
        print(f"Exploring {len(neighbors)} neighbors of score={best_score}...")

        for i, n in enumerate(neighbors):
            s = evaluate(n)
            total_evals += 1

            if s > best_score:
                best_score = s
                best = n
                improved = True
                print(f"  Improved to {s}! (neighbor {i}/{len(neighbors)})")
                print_puzzle(best, s)
                print()
                break  # Restart with new best

            if (i+1) % 500 == 0:
                elapsed = time.time() - start_time
                print(f"  Checked {i+1}/{len(neighbors)} neighbors ({elapsed:.1f}s, evals={total_evals})")

    # Phase 2: Extended random mutations
    print(f"\n=== Phase 2: Random mutations from score={best_score} ===\n")

    while time.time() - start_time < time_limit:
        p = dict(best)
        for _ in range(random.randint(1, 4)):
            p = mutate(p)

        s = evaluate(p)
        total_evals += 1

        if s > best_score:
            best_score = s
            best = p
            print(f"New best: {s} (evals={total_evals}, t={time.time()-start_time:.1f}s)")
            print_puzzle(best, s)
            print()

        if total_evals % 50000 == 0:
            print(f"evals={total_evals}, best={best_score}, t={time.time()-start_time:.1f}s")

    return best, best_score, total_evals


if __name__ == '__main__':
    print("=== Deep search from 28-step solution ===\n")
    print("Starting puzzle:")
    print_puzzle(BEST_28, 28)
    print()

    best, score, evals = deep_search(BEST_28, 28, time_limit=300)

    print(f"\n{'='*50}")
    print(f"FINAL BEST: {score} steps ({evals} evaluations)")
    print('='*50)
    print_puzzle(best, score)
