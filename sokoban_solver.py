"""
Sokoban puzzle solver and optimizer for 4x4 grid.

Rules:
- 4x4 grid with walls, floor, holes, one exit, one player, and blocks.
- Player wins by reaching the exit tile.
- Blocks can be pushed onto the exit (player doesn't win by pushing block onto exit).
  Player wins only when player steps onto exit.
- Blocks can have subsets of sides marked as "not pushable".
  A block with not-pushable sides cannot be pushed from those directions.
- Hole tiles consume blocks pushed into them (hole becomes floor).
  Player/blocks cannot enter a hole that hasn't been filled.
- A block cannot start on the exit tile.
- We want to maximize the length of the shortest solution.

Directions: 0=Up, 1=Right, 2=Down, 3=Left
"""

from collections import deque
from itertools import combinations, product
import time
import sys

# Directions: (dy, dx) for Up, Right, Down, Left
DIRS = [(-1, 0), (0, 1), (1, 0), (0, -1)]
OPPOSITE = [2, 3, 0, 1]  # opposite direction index

# Side labels for blocks: which side the push comes FROM
# If player is above block and pushes down, push comes from side 0 (Up/North side)
# pushable_sides is a frozenset of direction indices from which the block CAN be pushed

GRID_SIZE = 4

def in_bounds(r, c):
    return 0 <= r < GRID_SIZE and 0 <= c < GRID_SIZE


def solve_puzzle(walls, holes, exit_pos, player_start, blocks):
    """
    Solve a Sokoban puzzle using BFS.

    Args:
        walls: set of (r, c) positions that are walls
        holes: frozenset of (r, c) positions that are holes
        exit_pos: (r, c) position of exit
        player_start: (r, c) starting position of player
        blocks: tuple of (r, c, pushable_sides) where pushable_sides is a frozenset
                of direction indices from which the block can be pushed

    Returns:
        Length of shortest solution, or -1 if unsolvable
    """
    # State: (player_r, player_c, block_positions_frozen, holes_frozen)
    # block_positions_frozen: frozenset of (r, c, pushable_sides)
    # holes_frozen: frozenset of unfilled hole positions

    initial_blocks = frozenset((r, c, ps) for r, c, ps in blocks)
    initial_holes = holes

    start_state = (player_start[0], player_start[1], initial_blocks, initial_holes)

    visited = set()
    visited.add(start_state)
    queue = deque()
    queue.append((start_state, 0))

    # Precompute wall set for fast lookup
    wall_set = walls

    while queue:
        state, dist = queue.popleft()
        pr, pc, block_set, hole_set = state

        # Build a lookup for block positions
        block_pos_map = {}  # (r, c) -> pushable_sides
        for br, bc, bps in block_set:
            block_pos_map[(br, bc)] = bps

        for d in range(4):
            dr, dc = DIRS[d]
            nr, nc = pr + dr, pc + dc

            # Check bounds
            if not in_bounds(nr, nc):
                continue
            # Check walls
            if (nr, nc) in wall_set:
                continue
            # Check holes (can't walk into unfilled hole)
            if (nr, nc) in hole_set:
                continue

            # Check if there's a block at (nr, nc)
            if (nr, nc) in block_pos_map:
                bps = block_pos_map[(nr, nc)]
                # Check if block can be pushed from this direction
                # Player is at (pr, pc), moving in direction d to (nr, nc)
                # The push comes from direction d (the side the player is on)
                push_from = d
                if push_from not in bps:
                    continue  # Can't push from this side

                # Where would the block go?
                bnr, bnc = nr + dr, nc + dc

                # Check if block destination is valid
                if not in_bounds(bnr, bnc):
                    continue  # Block can't go out of bounds
                if (bnr, bnc) in wall_set:
                    continue  # Block can't go into wall
                if (bnr, bnc) in block_pos_map:
                    continue  # Block can't go into another block

                # Check if block goes into a hole
                if (bnr, bnc) in hole_set:
                    # Block consumed, hole filled
                    new_blocks = frozenset(
                        b for b in block_set if (b[0], b[1]) != (nr, nc)
                    )
                    new_holes = hole_set - {(bnr, bnc)}
                else:
                    # Block moves normally
                    new_blocks = frozenset(
                        (bnr, bnc, b[2]) if (b[0], b[1]) == (nr, nc) else b
                        for b in block_set
                    )
                    new_holes = hole_set

                # Player moves to where block was
                new_state = (nr, nc, new_blocks, new_holes)

                # Check if player reached exit
                if (nr, nc) == exit_pos:
                    return dist + 1

                if new_state not in visited:
                    visited.add(new_state)
                    queue.append((new_state, dist + 1))
            else:
                # No block, just move
                new_state = (nr, nc, block_set, hole_set)

                # Check if player reached exit
                if (nr, nc) == exit_pos:
                    return dist + 1

                if new_state not in visited:
                    visited.add(new_state)
                    queue.append((new_state, dist + 1))

    return -1  # Unsolvable


def enumerate_pushable_sides():
    """Enumerate all possible subsets of pushable sides (non-empty, since a block
    with no pushable sides is effectively a wall)."""
    sides = [0, 1, 2, 3]
    result = []
    for r in range(1, len(sides) + 1):
        for combo in combinations(sides, r):
            result.append(frozenset(combo))
    return result


def search_puzzles(max_blocks=3, max_holes=2, max_walls=6, verbose=True):
    """
    Search over puzzle configurations to find the one with the longest shortest solution.

    This uses a structured search with pruning.
    """
    all_positions = [(r, c) for r in range(GRID_SIZE) for c in range(GRID_SIZE)]
    all_pushable = enumerate_pushable_sides()

    best_solution = 0
    best_puzzle = None
    puzzles_tried = 0
    start_time = time.time()

    # Strategy: iterate over number of walls, then wall placements,
    # then exit, player, blocks, holes.
    # This is a huge search space, so we need heuristics.

    # Let's try a focused approach: fix fewer walls and more blocks with restricted sides

    for n_walls in range(0, max_walls + 1):
        for wall_combo in combinations(all_positions, n_walls):
            wall_set = frozenset(wall_combo)
            remaining = [p for p in all_positions if p not in wall_set]

            if len(remaining) < 3:  # Need at least exit, player, and some space
                continue

            for exit_pos in remaining:
                remaining2 = [p for p in remaining if p != exit_pos]

                for player_start in remaining2:
                    remaining3 = [p for p in remaining2 if p != player_start]

                    # Try different numbers of blocks
                    for n_blocks in range(0, min(max_blocks, len(remaining3)) + 1):
                        for block_pos_combo in combinations(remaining3, n_blocks):
                            remaining4 = [p for p in remaining3 if p not in block_pos_combo]

                            # For each block position, try different pushable sides
                            if n_blocks == 0:
                                block_side_combos = [()]
                            else:
                                block_side_combos = product(all_pushable, repeat=n_blocks)

                            for sides_combo in block_side_combos:
                                blocks = tuple(
                                    (block_pos_combo[i][0], block_pos_combo[i][1], sides_combo[i])
                                    for i in range(n_blocks)
                                )

                                # Try different numbers of holes
                                for n_holes in range(0, min(max_holes, len(remaining4)) + 1):
                                    for hole_combo in combinations(remaining4, n_holes):
                                        hole_set = frozenset(hole_combo)

                                        puzzles_tried += 1

                                        result = solve_puzzle(
                                            wall_set, hole_set, exit_pos,
                                            player_start, blocks
                                        )

                                        if result > best_solution:
                                            best_solution = result
                                            best_puzzle = {
                                                'walls': wall_set,
                                                'holes': hole_set,
                                                'exit': exit_pos,
                                                'player': player_start,
                                                'blocks': blocks,
                                                'solution_length': result
                                            }
                                            if verbose:
                                                elapsed = time.time() - start_time
                                                print(f"New best: {result} steps "
                                                      f"(tried {puzzles_tried} puzzles "
                                                      f"in {elapsed:.1f}s)")
                                                print_puzzle(best_puzzle)
                                                print()

                                        if puzzles_tried % 100000 == 0 and verbose:
                                            elapsed = time.time() - start_time
                                            print(f"Progress: tried {puzzles_tried} puzzles "
                                                  f"in {elapsed:.1f}s, best={best_solution}")

    return best_puzzle, best_solution, puzzles_tried


def print_puzzle(puzzle):
    """Print a puzzle configuration."""
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
        sides_str = ''.join(['U' if 0 in ps else '',
                            'R' if 1 in ps else '',
                            'D' if 2 in ps else '',
                            'L' if 3 in ps else ''])
        grid[br][bc] = f'B'

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
    print(f"Solution length: {puzzle['solution_length']}")


def smart_search(time_limit=300, verbose=True):
    """
    Smarter search using random sampling + local optimization.
    """
    import random

    all_positions = [(r, c) for r in range(GRID_SIZE) for c in range(GRID_SIZE)]
    all_pushable = enumerate_pushable_sides()

    best_solution = 0
    best_puzzle = None
    puzzles_tried = 0
    start_time = time.time()

    while time.time() - start_time < time_limit:
        # Random puzzle generation
        n_walls = random.randint(0, 8)
        positions = list(all_positions)
        random.shuffle(positions)

        walls = set(positions[:n_walls])
        remaining = positions[n_walls:]

        if len(remaining) < 3:
            continue

        exit_pos = remaining[0]
        player_start = remaining[1]

        n_blocks = random.randint(0, min(4, len(remaining) - 2))
        block_positions = remaining[2:2 + n_blocks]

        remaining2 = remaining[2 + n_blocks:]
        n_holes = random.randint(0, min(3, len(remaining2)))
        holes = frozenset(remaining2[:n_holes])

        blocks = tuple(
            (r, c, random.choice(all_pushable))
            for r, c in block_positions
        )

        result = solve_puzzle(frozenset(walls), holes, exit_pos, player_start, blocks)
        puzzles_tried += 1

        if result > best_solution:
            best_solution = result
            best_puzzle = {
                'walls': frozenset(walls),
                'holes': holes,
                'exit': exit_pos,
                'player': player_start,
                'blocks': blocks,
                'solution_length': result
            }
            if verbose:
                elapsed = time.time() - start_time
                print(f"New best: {result} steps (tried {puzzles_tried} in {elapsed:.1f}s)")
                print_puzzle(best_puzzle)
                print()

        if puzzles_tried % 10000 == 0 and verbose:
            elapsed = time.time() - start_time
            print(f"Random: tried {puzzles_tried} in {elapsed:.1f}s, best={best_solution}")

    return best_puzzle, best_solution, puzzles_tried


if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else 'random'

    if mode == 'random':
        print("=== Random Search ===")
        puzzle, solution, count = smart_search(time_limit=120, verbose=True)
    elif mode == 'exhaustive':
        print("=== Exhaustive Search (limited) ===")
        puzzle, solution, count = search_puzzles(
            max_blocks=2, max_holes=1, max_walls=4, verbose=True
        )

    if puzzle:
        print(f"\n=== BEST PUZZLE FOUND ===")
        print(f"Tried {count} puzzles")
        print_puzzle(puzzle)
    else:
        print("No solvable puzzle found")
