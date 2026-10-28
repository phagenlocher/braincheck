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
| [state-space.md](state-space.md) | `KState`, `KIterator`, `Kripke`: states, transitions, the observed atom |
| [memory-map.md](memory-map.md) | `MemoryMap`, the persistent memory inside a state |
| [reachability.md](reachability.md) | `check reach`: what the answer means and what it does not |
| [cell-visits.md](cell-visits.md) | `check visit`: pointer positions, visits before halting, counterexamples |
| [cell-values.md](cell-values.md) | `check value`: cell contents, comparisons, halting postconditions |
| [cell-overflow.md](cell-overflow.md) | `check overflow`: wraps past the maximum or zero, directions, counterexamples |
| [cli.md](cli.md) | commands, options, defaults, streams and exit codes |

Build, test and style conventions live in [AGENTS.md](../AGENTS.md). The
examples in `examples/` are documented in [examples/README.md](../examples/README.md).

## Rules that are easy to break

1. `KState::hash()` and `KState::compare()` must look at exactly the same fields.
   Different hashes for states that compare equal make the search's hash table
   grow without bound; equal states for different contents silently merge runs.
   See [state-space.md](state-space.md).
2. `MemoryMap` nodes are immutable and shared. Moving or writing a child of a
   node corrupts every snapshot that still points at it, and the damage is
   silent. See [memory-map.md](memory-map.md).
3. The observed atom must be a function of the state alone - instruction pointer,
   memory pointer, memory contents and the end point - and never of which
   successor is taken. A read's successors differ in memory and each is
   evaluated on its own contents, which is correct. See
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
10. A label **name** may occur at several pointers and counts at every one of
    them. The analyses look the name up per state (`Program::has_label_at()`)
    and scan the whole run of labels stored at that pointer, so one occurrence
    can never hide another. See [state-space.md](state-space.md).
11. A transition property is attached to the state it **leaves**, never to its
    successor. The overflow atom is true in the state whose instruction is the
    wrapping `+`/`-`. Marking the successor cannot work: it only shows the
    wrapped value and has forgotten which step produced it. See
    [cell-overflow.md](cell-overflow.md).
12. A universal check is decided through its negation, and the two negations are
    not interchangeable: `A F(atom)` needs a counterexample that avoids the atom
    from the *first* state, while `A G(F(atom))` accepts a prefix through the
    atom and only requires the cycle to avoid it. `search_always` and
    `search_eventually_always` implement exactly that difference (see
    [reachability.md](reachability.md), [cell-visits.md](cell-visits.md) and
    [state-space.md](state-space.md)).
