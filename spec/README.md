# braincheck specification

These files describe what braincheck *does*, as opposed to how it is built. They
are the reference you should read before changing behaviour: every rule below is
one that silently produces wrong answers when it is broken.

| File | Topic |
| --- | --- |
| [language.md](language.md) | source syntax, instructions, labels, parsing rules and errors |
| [memory.md](memory.md) | cells, tape, pointer and value normalisation (`MemoryModel`) |
| [input.md](input.md) | standard input: alphabet, pinned input, end of input, length budget (`IOModel`) |
| [execution.md](execution.md) | `execute` / `Program::run` interpreter semantics |
| [state-space.md](state-space.md) | `KState`, `KIterator`, `Kripke`: states, transitions, conditions |
| [memory-map.md](memory-map.md) | `MemoryMap`, the persistent memory inside a state |
| [reachability.md](reachability.md) | `check reach`: what the answer means and what it does not |
| [cell-visits.md](cell-visits.md) | `check visit`: pointer positions, visits before halting, counterexamples |
| [cell-values.md](cell-values.md) | `check value`: cell contents, comparisons, halting postconditions |
| [cell-overflow.md](cell-overflow.md) | `check overflow`: wraps past the maximum or zero, directions, witnesses |
| [cli.md](cli.md) | commands, options, defaults, streams and exit codes |

Build, test and style conventions live in [AGENTS.md](../AGENTS.md). The
examples in `examples/` are documented in [examples/README.md](../examples/README.md).

## Rules that are easy to break

1. `KState::hash()` and `KState::compare()` must look at exactly the same fields.
   Different hashes for states that compare equal make Spot's hash tables grow
   without bound; equal states for different contents silently merge runs.
   See [state-space.md](state-space.md).
2. `MemoryMap` nodes are immutable and shared. Moving or writing a child of a
   node corrupts every snapshot that still points at it, and the damage is
   silent. See [memory-map.md](memory-map.md).
3. A state condition must be a function of the state alone - instruction pointer,
   memory pointer, memory contents and the end point - and never of which
   successor is taken: Spot's Kripke product labels every transition leaving a
   state with that state's condition. A read's successors differ in memory and
   each carries its own condition, which is correct. See
   [state-space.md](state-space.md).
4. Never hardcode a cell modulus, a tape width or an input alphabet. Ask
   `MemoryModel`/`IOModel`. See [memory.md](memory.md) and [input.md](input.md).
5. Only a `,` consumes the input budget, and only a `,` may have more than one
   successor. See [input.md](input.md) and [state-space.md](state-space.md).
6. A cell holding zero is canonically absent from a state's memory. See
   [memory-map.md](memory-map.md).
7. There are no dead ends: the end point (`pc == instr_count()`) stutters. A dead
   end would hide its labels from `dot` and make a run that halts there invisible
   to `check reach`. See [state-space.md](state-space.md).
8. A folded block must never cross a label. The labelled pc is the only thing a
   run can observe, so folding it into the interior of a run hides it from
   `check reach`. See [state-space.md](state-space.md).
9. Never fold a property that can be observed. `check reach` observes labels; a
   cell-visit check observes the memory pointer, so it never folds pointer runs,
   and a cell-value or overflow check observes a cell's content and the step
   that changes it, so it never folds `+`/`-` runs. A folded `>>>` would hide
   the cells the pointer passes through; a folded `+++++` would hide every value
   it passes through and the single increment that wraps. See
   [cell-visits.md](cell-visits.md), [cell-values.md](cell-values.md),
   [cell-overflow.md](cell-overflow.md) and [state-space.md](state-space.md).
10. One atomic proposition per label **name**, not per label-map entry. A name
    that occurs at several pointers must produce one proposition that is true at
    each of them; a per-entry literal becomes `name & !name` at those pointers,
    their condition becomes `bdd_false()` and, because a product transition is
    `state_condition & automaton_condition`, those states lose every transition
    and all answers turn vacuous. See [state-space.md](state-space.md).
11. A transition property is attached to the state it **leaves**, never to its
    successor. Spot labels the transitions out of a state with that state's
    condition, so an overflow atom is true in the state whose instruction is the
    wrapping `+`/`-`. Marking the successor cannot work: it only shows the
    wrapped value and has forgotten which step produced it. See
    [cell-overflow.md](cell-overflow.md).
