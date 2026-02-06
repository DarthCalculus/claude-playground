"""
Verify the best puzzle and extract the full solution path.
"""

from collections import deque

GRID_SIZE = 4
DIRS = [(-1, 0), (0, 1), (1, 0), (0, -1)]
DIR_NAMES = ['Up', 'Right', 'Down', 'Left']

def in_bounds(r, c):
    return 0 <= r < GRID_SIZE and 0 <= c < GRID_SIZE


def solve_with_path(walls, holes, exit_pos, player_start, blocks):
    """BFS solver that returns the solution path."""
    initial_blocks = frozenset((r, c, ps) for r, c, ps in blocks)
    initial_holes = holes
    start_state = (player_start[0], player_start[1], initial_blocks, initial_holes)

    visited = {start_state: None}  # state -> (parent_state, direction)
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
                    # Reconstruct path
                    path = [d]
                    s = state
                    while visited[s] is not None:
                        parent, pd = visited[s]
                        path.append(pd)
                        s = parent
                    path.reverse()
                    return dist + 1, path

                if new_state not in visited:
                    visited[new_state] = (state, d)
                    queue.append((new_state, dist + 1))
            else:
                new_state = (nr, nc, block_set, hole_set)
                if (nr, nc) == exit_pos:
                    path = [d]
                    s = state
                    while visited[s] is not None:
                        parent, pd = visited[s]
                        path.append(pd)
                        s = parent
                    path.reverse()
                    return dist + 1, path

                if new_state not in visited:
                    visited[new_state] = (state, d)
                    queue.append((new_state, dist + 1))

    return -1, []


def simulate_and_display(walls, holes, exit_pos, player_start, blocks, path):
    """Simulate the solution step by step."""
    print("=== Solution Simulation ===\n")

    block_set = frozenset((r, c, ps) for r, c, ps in blocks)
    hole_set = holes
    pr, pc = player_start

    def display(step, direction=None):
        grid = [['.' for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]
        for r, c in walls:
            grid[r][c] = '#'
        for r, c in hole_set:
            grid[r][c] = 'O'
        er, ec = exit_pos
        grid[er][ec] = 'E'
        grid[pr][pc] = '@'
        for br, bc, bps in block_set:
            grid[br][bc] = 'B'

        if direction is not None:
            print(f"Step {step}: Move {DIR_NAMES[direction]}")
        else:
            print(f"Initial state:")
        for row in grid:
            print(' '.join(row))
        print()

    display(0)

    for step, d in enumerate(path):
        dr, dc = DIRS[d]
        nr, nc = pr + dr, pc + dc

        block_pos_map = {}
        for br, bc, bps in block_set:
            block_pos_map[(br, bc)] = bps

        if (nr, nc) in block_pos_map:
            bnr, bnc = nr + dr, nc + dc
            if (bnr, bnc) in hole_set:
                block_set = frozenset(b for b in block_set if (b[0], b[1]) != (nr, nc))
                hole_set = hole_set - {(bnr, bnc)}
                print(f"  [Block pushed into hole at ({bnr},{bnc})!]")
            else:
                block_set = frozenset(
                    (bnr, bnc, b[2]) if (b[0], b[1]) == (nr, nc) else b
                    for b in block_set
                )
                print(f"  [Block pushed from ({nr},{nc}) to ({bnr},{bnc})]")

        pr, pc = nr, nc
        display(step + 1, d)

        if (pr, pc) == exit_pos:
            print("*** PLAYER REACHED EXIT! ***")
            break


# Best puzzle: 28 steps
walls = frozenset()
holes = frozenset([(3,1), (3,2)])
exit_pos = (3,3)
player_start = (2,2)
blocks = (
    (1, 3, frozenset({0})),       # Up only
    (1, 1, frozenset({1,2,3})),   # Right, Down, Left
    (2, 3, frozenset({0,1,2})),   # Up, Right, Down
    (1, 2, frozenset({0})),       # Up only
    (0, 2, frozenset({0,1,2,3})), # All
)

print("=" * 60)
print("BEST SOKOBAN PUZZLE - 28 STEP OPTIMAL SOLUTION")
print("=" * 60)
print()
print("Grid (4x4):")
print(". . B .")
print(". B B B")
print(". . @ B")
print(". O O E")
print()
print("Legend: @ = Player, B = Block, O = Hole, E = Exit, . = Floor, # = Wall")
print()
print("Block details:")
print("  Block 0 at (1,3): pushable from Up only")
print("  Block 1 at (1,1): pushable from Right, Down, Left")
print("  Block 2 at (2,3): pushable from Up, Right, Down")
print("  Block 3 at (1,2): pushable from Up only")
print("  Block 4 at (0,2): pushable from All sides")
print()
print("Holes at: (3,1), (3,2)")
print("Exit at: (3,3)")
print("Player starts at: (2,2)")
print()

length, path = solve_with_path(walls, holes, exit_pos, player_start, blocks)
print(f"Verified solution length: {length}")
print(f"Solution path: {' '.join(DIR_NAMES[d] for d in path)}")
print()

simulate_and_display(walls, holes, exit_pos, player_start, blocks, path)
