# Reachability checking (`check reach`)

Implemented in `src/brainfuck/analysis.cpp` on top of
[the state space](state-space.md) and the searches in `search.{hpp,cpp}`.

## The question that is answered

```text
Is there a run that never visits a state whose instruction pointer carries LABEL?
```

"Run" means an infinite run. A program that halts stutters in its end point
(see [state-space.md](state-space.md)), so a terminating program also has an
infinite run: the finite run followed by the halted state forever. The question
is therefore:

```text
Does every run - finite or infinite - eventually visit the label?
```

The property is `A F(LABEL)` - "on every run, eventually LABEL". Its negation
is `E G(!LABEL)`: *some* run stays in the states that do not carry the label
forever. In a finite state graph an infinite run inside a set of states exists
exactly when that set contains a reachable cycle, so the implementation searches
for a cycle reachable through unlabelled states only
(`search_always(prog, !LABEL)` in `search.hpp`). A counterexample run exists
exactly when the answer is "no, the label is not guaranteed".

| Result | Meaning |
| --- | --- |
| `Label "<x>" will always be reached.` | no run avoids the label |
| `There exists a run for which the label "<x>" will not be reached:` | a counterexample run was found and is printed |

## What "always reached" means

It is a universal statement over all runs, and halting runs count:

- `[_x_+]+` skips the loop and halts. The label at `pc = 1` is never visited, so
  the answer is "will not be reached"; the counterexample is the run `[`, `+`
  and then the halted state forever.
- `+++_done_` visits the label at the end point and then stutters there, so the
  label is reached in every run: "always reached". A label at the end of a
  program therefore means "this program always halts".
- `+[]_end_` never halts and never reaches its label: a counterexample as well.

There is no vacuous case. A program that halts without visiting a label is a
counterexample because the halted state keeps the run alive; a program that
loops forever without the label is one because the loop is a cycle.

## Labels

- The label is matched by **name** against the program's labels.
- A name that does not occur anywhere is reported as
  `Label "<x>" does not exist in the specified program.` on stderr, with exit
  status `0`; no analysis is started.
- If the same name occurs at several instruction pointers, every occurrence
  counts, so the label is visited when *any* of them is.
- A label marks the state *before* the instruction it precedes: `_x_+` is true
  at the state whose `pc` is the `+`.
- A label behind the last instruction marks the end point, i.e. "the program has
  halted". It is printed by `print` and shown by `dot` like any other label.

## Input

`check reach` never reads standard input. The `IOModel` (see
[input.md](input.md)) describes which bytes a `,` may produce:

- unbounded input (default): every read may yield any of the 256 bytes, so a
  loop around a `,` usually has an infinite state space and the check may not
  terminate. There is no built-in timeout or step limit.
- `--max-stdin-length N`: the state space is bounded by the number of reads.
- `--eof-char` / `--no-change-on-eof` change what happens after the input ends.
- `--input BYTES`: the input is **fixed**. Every read has exactly one successor
  and the check answers for that one input instead of for every input. A fixed
  input can still loop forever, so a `,`-loop may still not terminate.

With `--input` the question changes from "is the label reached on every run over
all inputs?" to "is the label reached on every run over **these** bytes?". The
answer is still universal, but over a single input, so "always reached" no longer
means "reached for every possible input".

## The counterexample run

For a program without `,` there is exactly one run, so a counterexample is simply
the single run looping forever without the label. With input, it is one concrete
choice of bytes.

The printed run is the program text split into three parts:

1. the **prefix** (blue): from the initial state to the cycle;
2. the **cycle** (red): the repeating part, which proves the run is infinite;
3. the remainder of the program after the cycle (blue), with the requested label
   highlighted in yellow. This part is context only - it is not executed.

A state represents a whole folded block, so one step prints all instructions of
that block, and the remainder starts behind the last cycle block. A state at the
end point has no instruction, so it is printed as `<halt>`. A counterexample
whose cycle is `<halt>` is a run that stops without visiting the label (e.g.
`[_x_+]+` prints `[+` followed by `<halt>`).

The message, the program text and every colour escape go to **stdout**, so
redirecting stderr (`2>/dev/null`) does not change the counterexample.

The exit status is `0` for both answers; a non-zero status means the command
failed (parse error, bad option), not that the label is unreachable.

## Scope

The check is exact over the concrete state space (memory contents, input budget,
tape configuration), not over an abstraction: it does not merge states that were
reached by different paths and does not infer invariants. The input is part of
what is modelled, so `--input` narrows the question rather than weakening the
answer: every run over the pinned bytes is still explored exactly. It does fold
deterministic straight-line runs into one transition, including pointer runs,
because a label does not observe where the pointer is; this removes only
unlabelled interior states and leaves every labelled path intact (see
[state-space.md](state-space.md)). That is why the answer is trustworthy for the
modelled program and why large programs need the efficient state representation
described in [state-space.md](state-space.md) and
[memory-map.md](memory-map.md).

A check whose property observes the memory pointer cannot fold pointer runs;
see [cell-visits.md](cell-visits.md) for the `check visit` variant and its
different folding mode.
