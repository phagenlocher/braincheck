# State space: `KState`, `KIterator`, `Kripke`

Implemented in `src/brainfuck/kripke.{hpp,cpp}`. The program becomes a state
graph that the searches in `src/brainfuck/search.{hpp,cpp}` explore.

## What a state is

```text
KState = (pc, mem_ptr, memory, remaining_stdin_chars)
```

| Field | Type | Meaning |
| --- | --- | --- |
| `pc` | `instr_ptr_t` | instruction pointer; `pc == instr_count()` is the end point (the halted state) |
| `mem_ptr` | `mem_ptr_t` | memory pointer |
| `memory` | [`MemoryMap`](memory-map.md) | cells, **zero cells absent** |
| `remaining_stdin_chars` | `std::optional<std::size_t>` | reads left, `nullopt` = unbounded |

The initial state is `KState(io_model.get_chars_until_eof())`: `pc = 0`,
`mem_ptr = 0`, empty memory and the input budget from the `IOModel` (for a
pinned input, its length).

`pc` is normally the first instruction of a **block** (see
[Block folding](#block-folding)). The interior pointers of a folded run are
never states, but they still exist in the program's instruction list. With
`Folding::values` (used by cell-visit checks) pointer runs are not folded and
with `Folding::pointers` (used by cell-value and overflow checks) value runs are
not folded, so a state may also be an instruction inside a pointer or a value
run; see [Folding modes](#folding-modes).

A `KState` carries **no** `MemoryModel`. The model is needed only while a
`KIterator` is constructed; an iterator keeps no `Program` afterwards.

Copying a state is O(1) because the memory is shared, which is what lets the
searches keep a state per visited node. `get_memory_map()` returns the
`MemoryMap` by const reference and is what per-state code should use;
`get_memory()` materialises a `std::map` and exists for tests and debugging -
never call it in a path that runs per state.

## Transitions

`KIterator` computes the transition once, in its constructor, and records the
values it may write. `state()` then builds one successor on demand.

A state's `pc` is the first instruction of a block, so one transition applies
the whole block (see [Block folding](#block-folding)):

| Block | Successor `pc` | `mem_ptr` | Memory |
| --- | --- | --- | --- |
| run of `+`/`-` (length >= 1) | `block_end(pc)` | unchanged | cell = value after the run's net delta, wrapping at the cell width |
| run of `>`/`<` (length >= 1) | `block_end(pc)` | result of the whole run | unchanged |
| `,` | `pc + 1` | unchanged | one normalised possible character, **one successor per character** (up to 256, exactly one when the input is pinned) |
| `.` | `pc + 1` | unchanged | unchanged |
| `[` | `cell == 0 ? jump_target(pc) : pc + 1` | unchanged | unchanged |
| `]` | `cell != 0 ? jump_target(pc) : pc + 1` | unchanged | unchanged |
| end point (`pc == instr_count()`) | unchanged | unchanged | unchanged; exactly one successor, the state itself |

Details that matter:

- A pointer run is only one transition under `Folding::straight_line`. Under
  `Folding::values` every `>`/`<` is its own deterministic transition, so the
  pointer is observed on every cell it passes through, and under
  `Folding::pointers` a pointer run *is* one transition while every `+`/`-` is
  its own. The table above describes the folded case; a single step applies
  exactly one of `left`/`right` or `inc`/`dec`.
- A value run is only one transition under `Folding::straight_line` and
  `Folding::values`. Under `Folding::pointers` every `+`/`-` is its own
  deterministic transition, so the cell's content is observed at every
  intermediate value and the single increment or decrement that wraps is a
  state; a folded `+++++` would hide 1, 2, 3 and 4 from a cell-value check and
  the wrap from an overflow check.
- `,` is the **only** instruction with more than one successor. `+` and `-` are
  folded into a single deterministic transition, so they never branch.
- If the input model says "no change on end of input" and the budget is
  exhausted, `,` has exactly one successor that leaves the cell untouched
  (see [input.md](input.md)).
- Every read decrements `remaining_stdin_chars` in the successor state; no other
  instruction changes it. No block can contain a `,`, so folding never changes
  the input budget.
- A pinned input (`--input`) does **not** add a field to `KState`. The bytes are
  fixed for one exploration, so `remaining_stdin_chars` identifies the next one
  (`input[input.size() - remaining]`) and two states with the same count always
  see the same byte. Do not derive a state field from the input: it would break
  the "a state is fully described by its four fields" invariant below while
  `hash()`/`compare()` stayed unchanged.
- A transition that writes no cell still has exactly one successor. Internally
  the iterator stores a placeholder value for it and `writes_memory == false`;
  the placeholder is never read.
- A state at the end point is a program that has halted, and it **stutters**: it
  has exactly one successor, itself. The self-loop is deliberate and load
  bearing - it keeps the halted state observable, so a label behind the last
  instruction is shown by `dot` and a run that halts without visiting a label
  can still be reported as a counterexample (see
  [reachability.md](reachability.md)). Never turn the end point back into a dead
  end.
- `state()` must build **only** the successor for the current position. Building
  all successors up front is what made reads explode in cost.
- `first()` returns whether there is at least one successor, `next()` advances,
  and `done()` is `pos >= successor count`. An off-by-one in `done()` silently
  hides the last successor; `kripke_explores_every_input` guards this.

## Block folding

Consecutive `+`/`-` instructions are one block and consecutive `>`/`<`
instructions are another: only the net effect matters to a property that does
not observe the pointer. `Program` builds the block table once while parsing
(`Block`, `block_at()`, `block_end()` in `program.hpp`); it is immutable and
shared like the instruction list.

Folding happens in the **state graph only**. The parser keeps every
instruction, so `instr_count()`, `print`, `execute`, labels and jump targets are
unchanged and `Program::run()` still interprets one character at a time.

Rules the table obeys:

1. `+`/`-` and `>`/`<` never mix. A block is homogeneous.
2. A block never crosses a **label**. A labelled pc always starts a block, so
   the label state survives and the runs on both sides stay separate even when
   they have the same class (`+++_l_---` is two blocks).
3. `,`, `.`, `[` and `]` are single-instruction blocks. Folding never hides a
   read, an output or a branch.
4. Every **jump target is a block start**. Both jump targets are one past a
   bracket, and a bracket is not `+`/`-`/`>`/`<`, so the target always begins a
   block. The same holds for the successor pointer of every single-instruction
   block. Under `Folding::straight_line` therefore every reachable `pc` is a
   block start.

Soundness: each folded block is a chain of deterministic, unlabelled states with
exactly one successor. Contracting such a chain into one transition preserves
every labelled path, so `check reach` and `dot` see the same labels. Folding
does **not** merge states reached by different paths; it only removes the
interior of straight-line runs.

### Folding modes

`Folding` selects what may be folded:

| Mode | Folds | Used by |
| --- | --- | --- |
| `Folding::none` | nothing, one state per instruction | `dot --no-compress`, tests |
| `Folding::values` | `+`/`-` runs only | `check visit` |
| `Folding::pointers` | `>`/`<` runs only | `check value`, `check overflow` |
| `Folding::straight_line` | `+`/`-` and `>`/`<` runs | `check reach`, `dot`, `check value --on-halt` |

`Folding::values` exists because a cell-visit check observes the **memory
pointer**. Folding a `>>>` run would hide cells 1 and 2 from the predicate, so
pointer runs stay unfolded and every pointer position is a state. A state may
then be an interior instruction of a pointer block; `KIterator` must apply that
single `left`/`right` move, not the whole block returned by `block_at()`. Value
runs are still folded because they do not move the pointer. The block table is
unchanged and still describes the runs; only the graph chooses not to
apply a pointer block.

`Folding::pointers` is the mirror image. A cell-value check observes a cell's
**content**, an overflow check observes the **step that changes it**, and a
folded `+++++` would hide every intermediate value and the single increment that
wraps, so `+`/`-` runs stay unfolded while `>`/`<` runs are folded - a pointer
run changes no cell, so it hides nothing a value or overflow check can see. A
state may then be an interior instruction of a value block; `KIterator` applies
exactly one `inc`/`dec`.

`>`, `<` folding is exact on both tape modes. A wrapping tape is modular, so the
block's net displacement is enough. A clamping tape (`--no-wrapping`) loses
track of moves that hit an edge, so `build_blocks()` also composes the block's
result from cell `0` and from the last cell; `MemoryModel::move_pointer()`
clamps between them. Never replace that with plain `pointer + net`.

Value folding is exact because cell arithmetic always wraps at the cell width,
independently of the pointer's wrapping flag (see [memory.md](memory.md)).
`MemoryModel::shift_value()` is the helper; `build_blocks()` reduces the delta
modulo the cell width, so an arbitrarily long run cannot overflow the counter.

`Kripke(Program, Folding = straight_line, Observation = {})` controls folding
and what the analysis observes in a state (nothing, a cell's pointer position, a
cell content test, a content test restricted to the end point, or the wrap the
next step would cause). `check reach` uses `straight_line`; `check visit` uses
`values`; `check value` and `check overflow` use `pointers` (`check value
--on-halt` uses `straight_line`); `dot --no-compress` builds the unfolded
structure for debugging (one state per instruction).

## The observed atom

A check observes **one** atom in a state, and `Kripke::observes()` is where it
is evaluated:

```text
observes(pc, mem_ptr, memory) =
      mem_ptr == visited_cell                          // check visit
   OR (pc == instr_count() AND memory[cell] OP value)  // check value --on-halt
   OR (memory[cell] OP value)                          // check value
   OR next step wraps cell                             // check overflow
```

exactly one of the four branches is active, depending on the `Observation`, and
an empty observation observes nothing (`false`). Consequences:

- **Labels are not part of the atom.** A label check has no cell part: it looks
  the name up in the label map per state (`Program::has_label_at()`) and uses
  the result as its predicate. Several labels at one pointer all count there,
  and a label name at several pointers counts at each of them.
- **The atom is a function of the whole state.** It may look at `pc`,
  `mem_ptr` and the memory contents, but never at which successor is taken. A
  read is the case to watch: its successors differ in memory and each is
  evaluated on its own contents, which is correct because the atom belongs to
  the state the run is in, not to the edge.
- **A value check counts the end point** unless `on_halt` restricts it to
  `pc == instr_count()`. A halting program stutters there, so the atom stays
  true forever - which is what makes `--on-halt` a postcondition and
  `check visit --infinitely-often` fail on every terminating run (see
  [cell-visits.md](cell-visits.md) and [cell-values.md](cell-values.md)).
- **The wrap atom is true in the state *before* the wrapping `+`/`-`**, not in
  its successor: it looks at the instruction at `pc`, the memory pointer and the
  cell content, so it describes the transition leaving the state. The successor
  only shows the wrapped value and cannot say which step produced it (see
  [cell-overflow.md](cell-overflow.md)).
- `observes()` must be a function of `(pc, mem_ptr, memory)` only: do not let it
  depend on the input budget or on anything `hash()`/`compare()` cannot see, or
  the search merges states that are not the same.

## Invariants (breaking these silently corrupts the analysis)

1. **`hash()` and `compare()` must agree.** Both look at `pc`, `mem_ptr`,
   `remaining_stdin_chars` and the memory contents. Equal states must have equal
   hashes, and states with different memory must not compare equal. The hash is
   computed once in the constructor - do not make it depend on anything mutable.
2. **`compare()` must be a strict weak ordering.** The implementation orders by
   `pc`, then `mem_ptr`, then the optional input budget (`nullopt` first), then
   the memory (size, hash, then entries). Any consistent total order is fine;
   an inconsistent one makes the search's hash table lose or duplicate states.
3. **The observed atom must be a function of the state alone.** It may look at
   `pc`, `mem_ptr`, the memory contents and the end point, but never at which
   successor is taken. Never fold a pointer run when the property observes the
   pointer, and never fold a value run when the property observes a cell's
   content or the step that wraps it.
4. **Memory is canonical and immutable.** Zeros are absent; `KState` receives an
   already-canonical `MemoryMap` and never modifies it. See
   [memory-map.md](memory-map.md).
5. **A state is fully described by its four fields.** Do not add derived or
   cached state that `hash()`/`compare()` cannot see, and do not let ordering
   depend on anything else. The cached hash is derived, not new information.
6. **There are no dead ends.** The end point stutters, and every other state has
   a successor. A dead end would hide its labels from `dot` and make a run that
   stops there invisible to `check reach`.

## `Kripke`

- Holds a `Program` copy (O(1), see [language.md](language.md)), the folding
  mode and the observation.
- `Kripke(program, Folding = straight_line, Observation = {})` folds
  straight-line blocks by default (see [Block folding](#block-folding)).
  `Folding::none` gives the unfolded structure with one state per instruction
  and is used by `dot --no-compress` and the equivalence tests;
  `Folding::values` is used by cell-visit checks, `Folding::pointers` by
  cell-value and overflow checks.
- `initial_state()` returns a `KState` with the program's input budget.
  `pc = 0` is always a block start.
- `successors(state)` returns a `KIterator` for that state.
- `observes(state)` is the atom described above; it is `false` for a label
  check, which looks the label up itself.
- `get_program()`, `get_folding()` and `get_observation()` expose the pieces
  the searches and the printers need.
- `dot` labels a node with the labels written at its instruction pointer
  (`done`, `loop`, or `(none)`), plus `pc`, `mp` and the state hash. That only
  works because no state is a dead end (see the invariants above).
