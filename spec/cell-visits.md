# Cell visitation checks (`check visit`)

Implemented in `src/brainfuck/analysis.cpp` on top of
[the state space](state-space.md) and Spot.

## The questions that are answered

```text
check visit FILE CELL
    Does every run point at CELL at least once?            (F(CELL))

check visit FILE CELL --infinitely-often
    Does every run point at CELL infinitely often
    before it halts?                                       (G(F(!halt & CELL)))
```

"Run" means an infinite run, exactly as in [reachability.md](reachability.md).
A program that halts stutters in its end point, so it also has an infinite run.
The new part is that the end point carries a `halt` atom: an infinitely-often
check excludes it, so halting cannot masquerade as infinite visitation.

The answers are:

| Command | Result | Meaning |
| --- | --- | --- |
| `check visit` | `Cell N will be visited on every run.` | no run misses the cell |
| `check visit` | `There exists a run for which cell N will never be visited:` | a counterexample run is printed |
| `check visit --infinitely-often` | `Cell N will be visited infinitely often before halting on every run.` | every run is infinite and revisits the cell forever |
| `check visit --infinitely-often` | `There exists a run for which cell N will be visited only finitely often before halting:` | a counterexample run is printed |

## What "visited" means

The cell proposition is true at a state whose **memory pointer** is `CELL`.

- The initial state counts: the pointer starts on cell 0, so `check visit FILE 0`
  succeeds for every program.
- The value of the cell does not matter. The check never looks at a cell's
  contents, only at where the pointer is.
- Every pointer position is a state. A `>>>` run is **not** folded for this
  check; the interior cells become states, so the pointer really does "visit"
  them. This is the opposite of label reachability, where the pointer is
  invisible and folding pointer runs is safe.
- `+`/`-` runs are still folded because they do not move the pointer. Folding
  them removes no cell position.

## Before halting

The end point (`pc == instr_count()`) stutters. If it counted as a visit, every
halting program would visit its final cell infinitely often. That is not what
"before halting" means, so a cell-visit check adds the `halt` atom, which is
true exactly at the end point:

- `check visit` uses `F(CELL)`. The halting state **does** count here: `+>` moves
  the pointer to cell 1 and then halts, and that final move did reach cell 1.
- `check visit --infinitely-often` uses `G(F(!halt & CELL))`. The halting state
  does **not** count. A run that reaches the end point satisfies `halt` forever,
  so it cannot satisfy the inner `F(!halt & CELL)` from that point on.

Consequences:

- A halting program never satisfies the infinitely-often check, for any cell.
- Success of the infinitely-often check proves that **no run halts**, i.e. the
  program does not terminate on any run. To use it as a nontermination check,
  pick a cell that every iteration of the suspected loop must visit, such as the
  loop counter or the cell tested by `[...]`.
- An infinite run can still fail the check for a cell it leaves behind:
  `+>+[]` loops forever on cell 1, so `check visit FILE 1 --infinitely-often`
  succeeds while cell 0 is reported as visited only finitely often.

`halt` is an internal atomic proposition (`braincheck_halt`). It is registered
when a visit check runs and never appears in a source label: like the cell
proposition, its name contains an underscore, which the label parser never
produces.

## Counterexample runs

The printed run is the usual lasso:

1. the **prefix** (blue): the run up to the cycle;
2. the **cycle** (red): the repeating part;
3. the trailing source behind the cycle (blue), which is context only.

For a never-visited cell the run simply never points at `CELL`. For a finitely
visited cell the prefix may still visit it; the cycle is the part where it is
never visited again. A halting counterexample has `<halt>` as its cycle, which
shows directly that the run ends. A state is one pointer step or one folded
`+`/`-` run, so the printed text always matches the transitions that were
actually taken.

No source label is highlighted, because the property is about a cell index, not
a label.

## Memory and input

`check visit` accepts the same memory options as `check reach`
(`--memory-size`, `--cell-size`, `--no-wrapping`) and the same input options
(`--input`, `--max-stdin-length`, `--eof-char`, `--no-change-on-eof`). The
pointer behaviour and the tape size decide which cells can be visited at all,
and the input decides which branches are taken:

- With unbounded symbolic input, a loop around a `,` usually has an infinite
  state space and the analysis may not terminate.
- `--max-stdin-length N` bounds the number of reads.
- `--input BYTES` pins one input, so every read is deterministic and the answer
  is about that one input instead of every input. An infinitely-often success
  then proves nontermination for those bytes only, not for every possible
  input.

## Out-of-range cells

A cell outside `[0, memory_size)` cannot be visited. The command rejects the
index before analysing:

```text
Cell 5 does not exist on a tape of 4 cells (valid: 0..3).
```

The message goes to stderr and the exit status is `0`, mirroring an unknown
label in `check reach`. No analysis is started.

## Scope

The check is exact over the concrete state space (memory contents, input budget,
tape configuration), not over an abstraction. It does not merge states reached
by different paths and does not infer invariants. It folds `+`/`-` runs only,
which removes no pointer position and no labelled path. The `halt` atom is a
state condition, not a change to the transition system: the end point still
stutters, as required by [state-space.md](state-space.md).
