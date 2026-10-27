# Cell overflow checks (`check overflow`)

Implemented in `src/brainfuck/analysis.cpp` on top of
[the state space](state-space.md) and Spot.

## The question that is answered

```text
check overflow FILE CELL [--direction both|overflow|underflow]
    Does some run contain a step that wraps cell CELL?       (!G(wrap))
```

A **wrap** is the step that leaves the cell's range:

| direction | step | result |
| --- | --- | --- |
| overflow | `+` on a cell holding the maximum | `max + 1` becomes 0 |
| underflow | `-` on a cell holding zero | `0 - 1` becomes the maximum |

`--direction both` (the default) observes either wrap; `overflow` and
`underflow` narrow the observation to one of them.

The maximum comes from the configured cell width, so it is 255 for 8-bit,
65535 for 16-bit and 4294967295 for 32-bit cells. Cell arithmetic always wraps
at the cell width; `--no-wrapping` only affects the **pointer** and never
disables a cell wrap (see [memory.md](memory.md)).

The answers are:

| Command | Result | Meaning |
| --- | --- | --- |
| `check overflow` | `Cell N can wrap on some run:` | a witness run is printed |
| `check overflow` | `Cell N never wraps on any run.` | no run wraps the cell |
| `check overflow --direction overflow` | `... can overflow ...` / `... never overflows ...` | the same question for `+` only |
| `check overflow --direction underflow` | `... can underflow ...` / `... never underflows ...` | the same question for `-` only |

## Why the check is existential

There is no "every run" variant. A wrap is an event to hunt for - a bug, an
out-of-range result, a missed boundary - not a property that every run should
satisfy. Asking whether *every* run wraps would answer "no" with a
counterexample for almost every real program, and a program with an infinite
run that never wraps would be the counterexample even though it is the run that
matters least.

The check therefore always asks "can this cell wrap?" and reports either the
witness or that no run wraps it. The witness is the run that shows the event,
truncated where it happens.

## What is observed

A wrap is a **transition** property, not a state property. Holding the maximum
is not an overflow; the overflow is the `+` that crosses it. The Kripke
condition is therefore true in the state *before* that step:

```text
wrap(pc, mem_ptr, memory) =
      mem_ptr == CELL
  AND ( (instruction at pc is `+` AND memory[CELL] == maximum)   // overflow
      OR (instruction at pc is `-` AND memory[CELL] == 0) )      // underflow
```

with the direction filter applied afterwards. The state the wrap produces is
**not** marked: a cell holding zero or the maximum is indistinguishable from any
other and says nothing about how it got there.

Spot labels every transition leaving a state with that state's condition, which
is what makes a transition property expressible as a state condition (see
[state-space.md](state-space.md)). Consequences:

- **The initial state can already be a wrapping state.** A program that starts
  with `-` underflows cell 0 on its first step, because the initial cell holds
  zero.
- **The end point never wraps.** It contains no instruction, so a halting run
  that wraps the cell did so before it halted.
- **A read never wraps.** A `,` stores one input byte, which is at most 255 and
  therefore in range for every cell width. It can put a cell at the maximum,
  and the following `+` then wraps it.
- **Only the cell under the pointer can wrap.** `+` and `-` always act on the
  cell under the pointer, so a step whose pointer is elsewhere cannot wrap the
  observed cell.

## Direction

The witness stops at the first wrap of the selected direction and its marker
names the direction and the values:

```text
-+  <- cell 0 overflows (255 + 1 -> 0)
```

With the default `--direction both` the witness stops at the first wrap of
either kind, so it only proves the direction it shows. Ask
`--direction overflow` (or `underflow`) separately if you need to know whether
the other one can happen too. A program that only underflows a cell is reported
as never overflowing it when the direction is narrowed.

## Folding

A wrap is one `+` or one `-`, so `check overflow` uses `Folding::pointers`:
`>`/`<` runs are folded (they change no cell) and every `+`/`-` stays its own
state. A folded `++++…+` would hide the single increment that crosses the
maximum and the check would answer "never wraps".

The printed run is folding-aware, so one printed step always matches one
transition (see [state-space.md](state-space.md)).

## Witness runs

The witness is a lasso like every other analysis result, but it is truncated at
the first wrapping state: the last printed step is the `+` or `-` that wraps,
and the marker names the direction and the values:

```text
-+  <- cell 0 overflows (255 + 1 -> 0)
```

The rest of the run is not part of the answer. A program that never wraps has no
witness at all, so the answer is the plain sentence
`Cell N never wraps on any run.`

No source label is highlighted, because the property is about a cell, not a
label.

## Memory and input

`check overflow` accepts the same memory options as `check reach`
(`--memory-size`, `--cell-size`, `--no-wrapping`) and the same input options
(`--input`, `--max-stdin-length`, `--eof-char`, `--no-change-on-eof`). The cell
width decides the maximum, the tape size which cell indices are valid, and the
input which values a cell can hold before an increment or decrement:

- With symbolic input a `,` has up to 256 successors, one of which can put the
  cell at the maximum and make the next `+` wrap.
- `--input BYTES` pins one input, so every read is deterministic and the answer
  is about those bytes only.
- With unbounded symbolic input a loop around `,` usually has an infinite state
  space; bound it with `--max-stdin-length` or pin it with `--input`.

## Out-of-range cells

A cell outside `[0, memory_size)` cannot wrap. The command rejects the index
before analysing, exactly like `check visit` and `check value`:

```text
Cell 5 does not exist on a tape of 4 cells (valid: 0..3).
```

The message goes to stderr and the exit status is `0`. No analysis is started.

## Scope

The check is exact over the concrete state space (memory contents, input
budget, tape configuration), not over an abstraction. It does not merge states
reached by different paths and does not infer invariants.

A successful `--direction overflow` states that some run executes a `+` on a
cell holding the maximum. It does not claim the value is "too large" for
anything else: Brainfuck has no undefined overflow, only wrapping, so the wrap
itself is the whole observable event.
