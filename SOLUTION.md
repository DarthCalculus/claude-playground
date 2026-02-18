# Sokoban Longest Solution on 4x4 Grid

## Problem

Find a 4x4 Sokoban puzzle whose shortest solution is as long as possible, with these rules:
- No goal tiles; there is an exit tile the player wins by reaching
- Blocks can be pushed onto/off the exit tile
- Blocks cannot start on the exit tile
- Each block can have any subset of its sides designated as not pushable
- Hole tiles consume blocks pushed into them, becoming walkable

## Best Result: 32 Steps *(updated by genetic algorithm search)*

### Puzzle Layout

```
. B . O
. B B O
. . B E
B . @ O
```

- `@` = Player start (3,2)
- `E` = Exit (2,3)
- `B` = Blocks
- `O` = Holes
- `.` = Floor

### Block Details

| Block | Position | Pushable From |
|-------|----------|--------------|
| B0 | (2,2) | Up, Right |
| B1 | (1,1) | Up, Right, Down |
| B2 | (0,1) | Up, Right |
| B3 | (3,0) | All sides |
| B4 | (1,2) | Up only |

### Holes
- (0,3), (1,3), (3,3)

---

## Previous Best: 28 Steps

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
7. **Genetic Algorithm**: Population-based search (300 individuals) with crossover between parent puzzles — this broke the 28-step record, reaching 32 steps in ~30 seconds (~440k evaluations)

### How the Record Was Broken

The GA maintains a diverse population of 300 puzzles and generates offspring via **crossover** — combining blocks from one parent with exit/player/holes from another. This lets the search bridge across local optima that single-trajectory methods (SA, hill climbing) cannot escape. The 32-step puzzle was discovered at generation 1626 during a run that climbed 28 → 29 → 31 → 32 in rapid succession.

### Search Confidence (32-step result)

The 32-step solution is:
- Found by genetic algorithm crossover after >5M puzzle evaluations
- Emerged from combining features of multiple ~28-step local optima
- Uses a **6-block, 3-hole, no-wall** configuration — a region the prior searches did not deeply explore

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
- `sokoban_ga.c` - **Genetic algorithm** with crossover (broke the record: 28 → 32)
