# Cell value checks (`check value`)

Implemented in `src/brainfuck/analysis.cpp` on top of
[the state space](state-space.md) and Spot.

## The questions that are answered

```text
check value FILE CELL OP VALUE
    Does some run have a state where cell CELL holds a value
    OP VALUE?                                              (!G(cell !OP value))

check value FILE CELL OP VALUE --every-run
    Does every run have such a state?                      (F(cell OP value))

check value FILE CELL OP VALUE --on-halt
    Does some run halt with cell CELL holding a value
    OP VALUE?                                              (!G(!halt | cell !OP value))

check value FILE CELL OP VALUE --on-halt --every-run
    Does every run halt with such a value?                 (F(halt & cell OP value))
```

The default is **existential**: the check hunts for one run that reaches the
value and prints it as a witness. `--every-run` turns it into a universal check
and reports a counterexample instead. `--on-halt` moves the observation window
to the end point in both modes.

"Run" means an infinite run, exactly as in [reachability.md](reachability.md).
A program that halts stutters in its end point, so it also has an infinite run:

- Without `--on-halt` every state of the run is observed, the stuttering end
  point included. A halting program that reaches the value at its very end
  therefore does reach it.
- With `--on-halt` only the end point is observed, so the check becomes a
  **postcondition**: in `--every-run` mode it also fails when a run never
  halts, because that run has no end point at all.

`OP` is one of `=`, `>`, `>=`, `<` and `<=`.

The answers are:

| Command | Result | Meaning |
| --- | --- | --- |
| `check value` | `Cell N can reach ... on some run:` | a witness run is printed |
| `check value` | `Cell N never reaches ... on any run.` | no run reaches it |
| `check value --every-run` | `Cell N will reach ... on every run.` | no run misses the value |
| `check value --every-run` | `There exists a run for which cell N never reaches ...:` | a counterexample run is printed |
| `check value --on-halt` | `Cell N can hold ... when the program halts:` | a halting witness is printed |
| `check value --on-halt` | `Cell N never holds ... when the program halts on any run.` | no run halts with it |
| `check value --on-halt --every-run` | `Cell N will hold ... when the program halts on every run.` | every run halts with the value |
| `check value --on-halt --every-run` | `There exists a run for which cell N does not hold ... when the program halts:` | a counterexample run is printed |

`...` stands for `the value 7` for `=`, and for `a value >= 7` (and so on) for a
comparison.

## What is observed

The value proposition is true at a state whose **memory contents** satisfy the
comparison:

```text
memory[CELL] OP VALUE
```

- The cells are read from the state after a transition, so the value written by
  an instruction is observed in its successor state.
- Only cell content matters. The memory pointer does not, except through which
  cell the program writes.
- **The initial state counts.** All cells are zero before the first instruction,
  so `check value FILE CELL '=' 0` finds a witness immediately, exactly as
  `check visit FILE 0` succeeds through the initial pointer. There is no vacuous
  case.
- **The end point counts** unless `--on-halt` restricts the observation to it:
  the halted state keeps the memory the program finished with, and the default
  window looks at it like at any other state.
- A comparison is applied to the **normalised** content, so 8-bit wrapping
  counts: `-` from zero reaches 255, `+` from 255 reaches 0 (see
  [memory.md](memory.md)).

## Comparisons and out-of-range values

`OP` may be `=`, `>`, `>=`, `<` or `<=`. `VALUE` is parsed as an unsigned
64-bit number and is **not** restricted to the cell width, because a comparison
outside the cell range is still meaningful:

| Value | 8-bit cell |
| --- | --- |
| `= 300` | never satisfied (no cell can hold it) |
| `> 255` | never satisfied |
| `<= 300` | always satisfied |
| `< 0` | never satisfied |
| `>= 256` | never satisfied |

Rejecting such a value would turn an exact answer into a usage error, so the
check answers it instead. A value that no cell can hold is simply never
satisfied: the existential check reports no run, and `--every-run` finds a
counterexample.

## Folding

`check value` folds what it cannot see:

- Without `--on-halt` the content of one cell is observed in **every** state, so
  `+`/`-` runs stay unfolded (`Folding::pointers`). A folded `+++++` from 0 to 5
  would hide 1, 2, 3 and 4, and `check value FILE 0 '=' 3` must find a witness.
  Pointer runs *are* folded: a `>>>` run changes no cell content.
- With `--on-halt` only the end point is observed, so both run classes are
  folded (`Folding::straight_line`), exactly like `check reach`. A value that is
  only passed through on the way to the halting value is then not observed:
  `+++++` with `--on-halt` holds 5, not 3.

Folding never crosses a label ([state-space.md](state-space.md)), and the printed
run is folding-aware, so one printed step always matches one transition.

## Counterexample and witness runs

Both are lassos:

- A **witness** for the default check is truncated at the first state that
  satisfies the comparison, and that state is marked:

  ```text
  +++[>+++<-]>+++<-]  <- cell 1 = 9
  ```

  The rest of the run is not part of the answer, so a witness is a shortest
  printed prefix rather than a whole lasso.
- A **counterexample** for `--every-run` is a run in which the comparison never
  holds; the printed prefix, cycle and trailing context are the same as for
  `check visit`. A halting counterexample has `<halt>` as its cycle.
- For `--on-halt` the state that satisfies the comparison must be the end point,
  so the witness ends in `<halt>` and a counterexample is either an endless run
  or a run that halts with a value that fails the comparison.

No source label is highlighted, because the property is about a cell, not a
label.

## Memory and input

`check value` accepts the same memory options as `check reach`
(`--memory-size`, `--cell-size`, `--no-wrapping`) and the same input options
(`--input`, `--max-stdin-length`, `--eof-char`, `--no-change-on-eof`). The cell
width decides which values are representable, the tape size which cell indices
are valid, and the input which branches are taken:

- With unbounded symbolic input a `,` has up to 256 successors with different
  contents, so a value check on an input-driven loop often has a large or
  infinite state space.
- `--max-stdin-length N` bounds the number of reads.
- `--input BYTES` pins one input, so every read is deterministic and the answer
  is about those bytes only.

## Out-of-range cells

A cell outside `[0, memory_size)` cannot hold a value. The command rejects the
index before analysing:

```text
Cell 5 does not exist on a tape of 4 cells (valid: 0..3).
```

The message goes to stderr and the exit status is `0`, mirroring an unknown
label in `check reach`. No analysis is started.

## Scope

The check is exact over the concrete state space (memory contents, input budget,
tape configuration), not over an abstraction. It does not merge states reached
by different paths and does not infer invariants. Success of
`--on-halt --every-run` is a total-correctness statement - the program terminates
on every run *and* every run ends with the requested value - which `check reach`
alone cannot express.

A cell that holds the maximum or zero has not wrapped yet: `check value` observes
the content, while [`check overflow`](cell-overflow.md) observes the `+`/`-` that
crosses the boundary.
