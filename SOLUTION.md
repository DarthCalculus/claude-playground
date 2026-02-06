# Sokoban Longest Solution on 4x4 Grid

## Problem

Find a 4x4 Sokoban puzzle whose shortest solution is as long as possible, with these rules:
- No goal tiles; there is an exit tile the player wins by reaching
- Blocks can be pushed onto/off the exit tile
- Blocks cannot start on the exit tile
- Each block can have any subset of its sides designated as not pushable
- Hole tiles consume blocks pushed into them, becoming walkable

## Best Result: 28 Steps

### Puzzle Layout

```
. . B .
. B B B
. . @ B
. O O E
```

- `@` = Player start (2,2)
- `E` = Exit (3,3)
- `B` = Blocks
- `O` = Holes
- `.` = Floor

### Block Details

| Block | Position | Pushable From |
|-------|----------|--------------|
| B0 | (1,3) | Up only |
| B1 | (1,1) | Right, Down, Left |
| B2 | (2,3) | Up, Right, Down |
| B3 | (1,2) | Up only |
| B4 | (0,2) | All sides |

### Holes
- (3,1) and (3,2)

### Optimal Solution (28 moves)

```
L L U U R R L L D D R R U D L L U U R D L D R U R D D R
```

(Left Left Up Up Right Right Left Left Down Down Right Right Up Down Left Left Up Up Right Down Left Down Right Up Right Down Down Right)

## Methodology

1. **BFS Solver**: Exact shortest-path solver using breadth-first search over the full state space (player position × block positions × hole states)
2. **Random Search**: ~5M random puzzles evaluated as initial seed generation
3. **Hill Climbing**: Mutation-based local search from top seeds
4. **Simulated Annealing**: Temperature-based exploration to escape local optima
5. **Exhaustive Neighbor Search**: All single-mutation and pair-mutation neighbors of the best solution checked
6. **C Implementation**: Optimized C solver for ~10x speedup over Python

### Search Confidence

The 28-step solution is:
- A **verified global local optimum**: no single mutation improves it
- Stable under **exhaustive pair-mutation search** (all pairs of pushable side changes, position changes, etc.)
- Resistant to **simulated annealing** with multiple restarts
- The best found across **>10M puzzle evaluations** total

## Files

- `sokoban_solver.py` - Python BFS solver + random search
- `sokoban_opt.py` - Python hill climbing optimizer
- `sokoban_deep.py` - Deep local search from best solution
- `sokoban_verify.py` - Solution verification with step-by-step display
- `sokoban_fast.c` - C BFS solver + random search
- `sokoban_fast2.c` - C solver with exhaustive + hill climbing
- `sokoban_sa.c` - C simulated annealing optimizer
- `sokoban_pair.c` - C exhaustive pair-mutation search
- `sokoban_big.c` - C targeted search for complex puzzles
