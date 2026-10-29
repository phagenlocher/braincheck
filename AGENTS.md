# AGENTS.md

Guidance for AI agents working in this repository.

## General

Whenever planing or doing changes, check if _ANY_ documentation needs changes, be it AGENTS.md, specifications in specs/, or any code documentation. If so, add it to your plan.

## Project

braincheck is a work-in-progress static/dynamic analyzer for the esoteric
programming language Brainfuck (GPL-3.0). A parsed program induces a finite
state graph (`KState`, `KIterator`), and assertions are answered by a direct
search over that graph (`src/brainfuck/search.{hpp,cpp}`): reachability for an
existential check, a cycle search for a universal one. Today label
reachability, cell visitation (at least once, and infinitely often before
halting), cell values (equality and comparisons, as a reachability question, an
existential question or a postcondition at halting) and cell overflow/underflow
(a wrap of the transition, in either direction) are implemented.

Language: C++17 (+ a little C in the vendored dependencies). Build system:
CMake >= 3.27. There is **no model-checking library dependency**: the program
has no runtime dependencies at all and no submodule besides CLI11 and minunit.

## Specification

The behavioural specification lives in [`spec/`](spec/README.md). It is
normative: read the file for the area you are about to change before touching
it, because most of its rules fail silently when broken.

| File | Topic |
| --- | --- |
| [spec/language.md](spec/language.md) | source syntax, instructions, labels, parsing rules and errors |
| [spec/memory.md](spec/memory.md) | cells, tape, pointer, value normalisation (`MemoryModel`) |
| [spec/input.md](spec/input.md) | standard input: alphabet, end of input, length budget (`IOModel`) |
| [spec/execution.md](spec/execution.md) | `execute` / `Program::run` interpreter semantics |
| [spec/state-space.md](spec/state-space.md) | `KState`, `KIterator`, `Kripke`: states, transitions, the observed atom |
| [spec/memory-map.md](spec/memory-map.md) | `MemoryMap`, the persistent memory inside a state |
| [spec/reachability.md](spec/reachability.md) | `check reach`: what the answer means and what it does not |
| [spec/cell-visits.md](spec/cell-visits.md) | `check visit`: pointer positions, visits before halting, counterexamples |
| [spec/cell-values.md](spec/cell-values.md) | `check value`: cell contents, comparisons, halting postconditions |
| [spec/cell-overflow.md](spec/cell-overflow.md) | `check overflow`: wraps past the maximum or zero, directions, counterexamples |
| [spec/cli.md](spec/cli.md) | commands, options, defaults, streams and exit codes |

`spec/README.md` also lists the invariants that are most easily broken.

## Build and test

The build is **in-source**: configure at the repository root, and the
`braincheck` executable is written next to the root `CMakeLists.txt`.

```sh
git submodule update --init --recursive   # CLI11, minunit
cmake .                                   # configure (fast, no vendored build)
make                                      # builds braincheck
make test                                 # builds and runs the minunit tests
```

System dependencies (also installed by CI): `cmake` and `g++` (C++17). Nothing
else: there is no autotools/bison/flex/patch step and no library to bootstrap.

Notes:

- There is **no CTest integration**. `test` is a custom target that builds
  `tests` and runs it; `src/tests.cpp` uses minunit and exits with the minunit
  exit code.
- `CMAKE_EXPORT_COMPILE_COMMANDS` is ON, so every `cmake .` writes
  `compile_commands.json` next to the root `CMakeLists.txt` (gitignored) and
  clangd/editors resolve the target include paths (`src/brainfuck`,
  `src/argparse`, `src/extern/CLI11`, ...). Re-run `cmake .` after
  adding source files or targets, then restart the language server.
- Compiler flags are strict: `-Wall -Wextra -pedantic -Werror` plus `-O3`.
  Any new warning breaks the build. The executable is linked statically and
  fully stripped after the build; `tests` is built with `-g` and keeps its
  debug info.
- Build output (`CMakeCache.txt`, `CMakeFiles/`, the `braincheck` binary) is
  gitignored. Never commit generated files.

## Usage

```sh
./braincheck                          # help
./braincheck --version
./braincheck execute prog.bf [--memory-size N] [--cell-size 8|16|32] [--no-wrapping]
./braincheck print prog.bf [--no-labels]
./braincheck dot prog.bf [--memory-size N] [--cell-size 8|16|32] [--no-wrapping]
./braincheck check reach prog.bf LABEL \
    [--memory-size N] [--cell-size 8|16|32] [--no-wrapping] \
    [--input BYTES] [--max-stdin-length N] [--eof-char C] [--no-change-on-eof]
./braincheck check visit prog.bf CELL \
    [--infinitely-often] \
    [--memory-size N] [--cell-size 8|16|32] [--no-wrapping] \
    [--input BYTES] [--max-stdin-length N] [--eof-char C] [--no-change-on-eof]
./braincheck check value prog.bf CELL OP VALUE \
    [--every-run] [--on-halt] \
    [--memory-size N] [--cell-size 8|16|32] [--no-wrapping] \
    [--input BYTES] [--max-stdin-length N] [--eof-char C] [--no-change-on-eof]
./braincheck check overflow prog.bf CELL \
    [--direction both|overflow|underflow] \
    [--memory-size N] [--cell-size 8|16|32] [--no-wrapping] \
    [--input BYTES] [--max-stdin-length N] [--eof-char C] [--no-change-on-eof]
```

`execute` accepts the same input options as `check reach`, `check visit`,
`check value` and `check overflow`: `--input BYTES` pins the bytes a `,` reads
(`\n`, `\t`, `\r`, `\0`, `\\` and `\xNN` escapes) instead of reading standard
input or exploring every byte. `dot` has no input options.

Analysis checks live below the `check` command group; `reach`, `visit`, `value`
and `overflow` are its subcommands, and a bare `./braincheck check` prints the
group's help. `check visit` observes the memory pointer, so it folds `+`/`-` runs
but never pointer runs; `check visit --infinitely-often` excludes the stuttering
end point and therefore means "infinitely often before halting" (see
`spec/cell-visits.md`).

`check value` compares one cell's content: `OP` is `=`, `>`, `>=`, `<` or `<=`
and `VALUE` may exceed the cell width (an 8-bit cell is always `<= 300`). By
default some run must reach a state where the test holds, and a witness that
stops there is printed; `--every-run` requires every run to reach it and reports
a counterexample instead. `--on-halt` observes only the end point, which turns
the check into a postcondition in `--every-run` mode. It folds pointer runs but
never `+`/`-` runs, because every intermediate value is observable (see
`spec/cell-values.md`).

`check overflow` verifies that no run wraps a cell: `+` on the maximum
(overflow) or `-` on zero (underflow). It is a safety property (`A G(!wrap)`),
so the green answer is "never wraps" and a run that wraps is printed as a
counterexample, truncated on the wrapping instruction. `--direction` narrows to
one direction and the default observes both. It folds nothing but pointer runs,
because the wrapping step is a single `+` or `-` inside a value run (see
`spec/cell-overflow.md`).

Labels are written inline in the source as `_labelname_` and mark the current
instruction pointer; `check reach` prints a counterexample run (prefix +
cycle) when the label is not always reached.

`execute`, `dot` and `check reach` take their memory configuration from the
model objects; the flags only override the model's own defaults.

## Repository layout

- `src/main.cpp` — entry point. Defines the `argparse` callback lambdas
  (`exfun`, `pfun`, `dotfun`, `reachfun`, `visitfun`, `valuefun`,
  `overflowfun`) and forwards to `argparse::run_with_args`. Also holds the ANSI
  color macros, the folding-aware counterexample and witness printers and the
  Graphviz writer (`print_dot`) used for user-facing output.
- `src/argparse/` — CLI11 front end (`argparse.cpp`, `argparse.hpp`).
  Subcommands are registered here; work is delegated to the function pointers,
  never done inline. Keep argument parsing and analysis logic separated this
  way.
- `src/brainfuck/` — the core static library `brainfuck`:
  - `program.{hpp,cpp}` — parsing from file/stream into `vector<Instruction>`,
    jump map construction, label map, interpreter (`run()`), `print()`, and the
    straight-line block table (`Block`, `block_at`, `block_end`) used by the
    state graph. The instruction list, labels, jump targets and blocks are
    immutable and shared, so a `Program` copy is O(1).
  - `model.{hpp,cpp}` — `MemoryModel` (cell size 8/16/32 bit, wrapping
    pointer, sparse `std::map` memory) and `IOModel` (input alphabet, EOF
    character, optional input length bound).
  - `memory_map.{hpp,cpp}` — `MemoryMap`, the immutable persistent sparse map
    a `KState` stores its memory in.
  - `kripke.{hpp,cpp}` — the state graph: `KState` (pc, memory pointer, memory,
    remaining stdin), `KIterator` (lazy successor generation, folding modes) and
    `Kripke` (initial state, successors, the `Observation` atom, the `Folding`
    mode). `Kripke::observes()` is the atom a check observes in a state; a
    label check uses the label map and observes nothing here.
  - `search.{hpp,cpp}` — the three graph searches every check is built on:
    `search_eventually` (`E F(P)`, a shortest witness path), `search_always`
    (`E G(P)`, a counterexample lasso inside the predicate) and
    `search_eventually_always` (`E F(G(P))`, a lasso whose prefix may leave the
    predicate). Each contract states the LTL formula it decides and why the
    graph search is equivalent - keep those arguments in sync with
    `spec/state-space.md`.
  - `analysis.{hpp,cpp}` — `check_reach`, `check_visit` (`VisitKind`),
    `check_value` (`ValueKind`, `CellTest`) and `check_overflow` (`WrapTest`):
    each states the LTL property it verifies, negates it and hands the
    resulting predicate to one of the three searches.
  - `brainfuck.hpp` — umbrella header; `main.cpp` and `tests.cpp` include only
    this.
- `src/tests.cpp` — minunit test suites (`parsing`, `models`, `memory_maps`,
  `hashing`, `blocks`, `kripke`, `analysis`, `cell_visits`, `cell_values`,
  `cell_overflows`, `search`).
- `spec/` — the behavioural specification (start at `spec/README.md`). Informative
  for users, normative for contributors.
- `src/extern/` — git submodules (`CLI11`, `minunit`), both header-only.
  **Never edit these.**
- `.github/workflows/cmake-build-and-test.yml` — CI: submodules, `cmake .`,
  `make`, `make test`.

## Code conventions

- C++17, `#pragma once`, 4-space indentation, braces on their own line for
  functions and classes, `namespace brainfuck` for library code.
- Naming: types `PascalCase` (`Program`, `MemoryModel`, `KState`), functions and
  methods `camelCase` (`parse_from_istream`, `get_possible_chars`), fields
  `snake_case` (`label_map`, `chars_until_eof`), pointer typedefs
  `instr_ptr_t` / `mem_ptr_t`.
- `Instruction` enum members are lowercase (`left`, `right`, `inc`, `dec`,
  `get`, `put`, `fwd`, `bwd`); `instr_char()` is the single mapping back to
  source characters. Add new instructions in both places.
- **The models are the only source of truth.** Cell size, tape size, pointer
  wrapping and the input alphabet live in `MemoryModel`/`IOModel`. Never
  hardcode a cell modulus, a tape width or a 30000 anywhere: ask the model
  (`get_max_value()`, `get_memory_size()`, `is_wrapping()`) or use its pure
  transition helpers `pointer_left/pointer_right/next_value/prev_value/
  normalize_value/shift_value/move_pointer`. The interpreter (`Program::run`)
  and the state graph (`KIterator`) both go through those helpers and must
  stay in sync.
- Cell values are `cell_value_t` (currently `uint32_t`, the widest cell) and
  input/output characters are `io_char_t` (`uint8_t`). Do not narrow state
  memory to `uint8_t` — that silently disables 16/32-bit cells.
- Library objects are passed and copied by value (e.g. `Program`, `KState`, and
  the models handed to the `argparse` callbacks); this is intentional — don't
  switch to pointers or references without a reason. `Program` copies are cheap
  because its immutable tables are shared. `KIterator` takes the `Program` by
  const reference because it only reads it while the iterator is constructed.
- Parse failures throw `brainfuck::ParseException`; user-facing errors are
  printed with the color macros and terminate via `exit(1)` in `main.cpp`.
- No formatter or linter config exists. Match surrounding style rather than
  reformatting whole files.

### Documentation

- Every type, function, method, constructor, enum, typedef and non-obvious
  constant carries a Doxygen comment: `//!` for a one-line brief, `//!` plus a
  `/** ... */` block when the contract needs more than one line.
- Document declarations in the header, not the definition, so the contract
  lives next to the API and cannot drift between the two. Only file-local
  helpers and implementation details in `.cpp` files are documented there.
- Use `\a name` for parameter references and `\param`, `\return`, `\pre` and
  `\post` when they add information. Explain *why* and the contract, not the
  obvious mechanics of the statement below.
- Never duplicate the normative rules from [`spec/`](spec/README.md). State the
  local contract and link the spec page instead, so a behaviour change updates
  one place.
- Each `MU_TEST` gets a one-line comment saying what it guarantees; the test
  name alone is not enough to see which invariant it guards.
- Comments are English and wrapped like the surrounding code.

## Testing

- Tests use minunit. Add `MU_TEST(name) { ... }`, then reference it in an
  `MU_TEST_SUITE(...)` block, and make sure the suite is run from `main()` in
  `src/tests.cpp`. A test that is not referenced by a suite is silently
  skipped.
- Prefer testing against `Program::parse_from_istream` with a
  `std::istringstream` instead of files on disk.
- Run `make test` before submitting changes; CI runs exactly the same command.

## Known rough edges

- `KIterator::done()` must stay `pos >= possible_values.size()`. It used to be
  `pos >= states.size() - 1`, which hid the last successor (and every successor
  of deterministic instructions); `kripke_explores_every_input` guards this.
- `KState::hash()` and `KState::compare()` must stay consistent. Both use `pc`,
  memory pointer, remaining input length *and* the memory contents; a mismatch
  makes the search's hash table store states that compare equal, so exploring a
  program with an infinite loop grows without bound and crashes. Zero cells are
  canonicalised away by `MemoryMap` for the same reason. The hash is computed
  once in the `KState` constructor because the search asks for it once per
  visited state and the fields never change afterwards.
- **`MemoryMap` nodes are immutable and shared.** Never move, write or otherwise
  mutate a child of a node: the previous snapshots still point at it, and the
  damage is silent (a state loses a cell and the analysis answers nonsense).
  The node members are `const` so that a `std::move` of a child degrades into a
  copy instead of stealing it. `memory_map_matches_std_map` checks after every
  operation that the snapshot an operation started from is unchanged.
- Memory updates are persistent (path copying), so a successor shares all but
  O(log n) nodes with its parent. Do not "optimise" this back into a
  `std::map` copy per state: that is quadratic in the number of live cells and
  was the main reason large programs needed gigabytes and minutes.
- `KState` does not carry a `MemoryModel`. The model is only used while a
  `KIterator` is constructed; the iterator keeps no `Program` afterwards. Every
  state in one exploration shares the same model, so hashing and ordering stay
  valid.
- `KIterator` generates successors on demand: the constructor computes the
  transition and the values it may write, `state()` materialises one state. Do
  not build all successors eagerly again - a read has 256 of them.
- **Blocks fold straight-line runs; they do not merge states reached by
  different paths.** `Program` builds one `Block` per instruction pointer at
  parse time. A block is a maximal run of `+`/`-` or `>`/`<`, always stops
  *before* a labelled pc, and contains no `,`/`.`/`[`/`]`. Never fold across a
  label: the labelled pc must stay a state. Every jump target and every
  successor pc is a block start under `Folding::straight_line`, because both
  jump targets are one past a bracket; `blocks_tile_the_program_and_keep_jump_targets`
  guards it.
- **`Folding` decides what may be folded, and a property must not fold what it
  observes.** `Folding::straight_line` (labels, `dot`) folds `+`/`-` and `>`/`<`
  runs. `Folding::values` (cell visits) folds `+`/`-` only, because the memory
  pointer is observable; a state may then be an interior instruction of a
  pointer block, and `KIterator` must apply one `left`/`right` move instead of
  the whole block from `block_at()`. Never fold a pointer run for a cell check:
  `>>>` would hide cells 1 and 2. `Folding::pointers` (cell values and
  overflow) is the mirror image: it folds `>`/`<` only, because a cell's content
  and the step that changes it are observable, so a folded `+++++` would hide
  every intermediate value and the single increment that wraps; a state may then
  be an interior instruction of a value block. `Folding::none` is the fully
  unfolded debugging structure. `check value --on-halt` observes only the end
  point and may fold everything.
- **Pointer blocks on a clamping tape are not their net displacement.**
  `--no-wrapping` loses moves that hit an edge, so `build_blocks()` stores
  `low`/`high` (the block's result from cell 0 and from the last cell) and
  `MemoryModel::move_pointer()` clamps between them. Plain `pointer + net` is
  wrong: `>><` from cell 1 on a three-cell tape ends on 1, not 2. Value runs are
  unaffected because cell arithmetic always wraps at the cell width.
- The counterexample printer in `main.cpp` is folding-aware: a step prints the
  whole transition it stands for. `check reach` passes
  `Folding::straight_line` and prints a whole block; `check visit` passes
  `Folding::values`, so a pointer step prints one instruction and a value step
  prints the whole run; `check value` and `check overflow` pass
  `Folding::pointers` (`check value --on-halt` passes `straight_line`), so a
  value step prints one instruction and a pointer step the whole run. Printing
  one character per state would drop the rest of a folded run and duplicate it
  in the context; printing a whole block per unfolded state would do the same
  for the other class. `print_truncated_run()` truncates a run at the first
  state a value witness or an overflow counterexample is looking for and marks
  it with the text its marker callable returns; `dot --no-compress` shows the
  fully unfolded structure.
- The block table encodes the tape size (pointer clamp bounds) and the cell
  width (reduced value delta). Like `set_cell_size()`, it is fixed at parse
  time; configure the models before parsing and do not mutate them afterwards.
- The end point (`pc == instr_count()`) is a real state with a self-loop: the
  program has halted and stutters there. That is what makes a label behind the
  last instruction visible to `dot` and lets `check reach` report a run that
  halts without visiting a label (`Program::is_end()` names the point). Never
  turn it back into a dead end.
- Brainfuck has no comment syntax: every `+ - < > . , [ ]` character in a
  source file is an instruction, including the ones inside prose. The files in
  `examples/` keep their prose free of those characters for that reason.
- **A label name may occur at several pointers, and every occurrence counts.**
  Labels are a `std::multimap` keyed by pointer, so `has_label_at()` scans the
  whole run of equal keys instead of looking at the first entry. A label check
  used to turn a repeated name into `name & !name`; today the label is looked up
  per pointer, and `check_visit_duplicate_label_name` and
  `check_value_duplicate_label_name` guard that neither occurrence is lost.
- **`Kripke::observes()` is the only atom a check observes.** It is false for a
  label check (labels are looked up in the label map) and otherwise exactly one
  of: the pointer position (`mem_ptr == cell`), the cell content
  (`cell_test_holds()` plus the end point when `on_halt`), or the wrap
  (`wrap_test_holds()`). Keep it a function of `(pc, mem_ptr, memory)` only:
  a predicate that depends on the input budget or on anything `hash()`/`compare()`
  cannot see makes the search merge states that are not the same.
- **A wrap is a transition, so its predicate is evaluated in the state it
  leaves.** The wrap atom is true where the instruction at `pc` is the wrapping
  `+`/`-`, the pointer is on the observed cell and the content is the maximum
  (overflow) or zero (underflow). Marking the successor cannot work: it only
  shows the wrapped value and no longer knows which step produced it. The
  successor is also indistinguishable from any other state holding that value.
  `spec/README.md` rule 11 and `spec/cell-overflow.md` state this.
- **The three searches decide three different LTL shapes, and mixing them up is
  a wrong answer rather than a slow one.**
  - `search_eventually(k, P)` decides `E F(P)` and returns the shortest path
    (BFS, stops at the first hit). Used by `check value` (default) as a witness
    and by `check overflow`, which verifies the dual `A G(!P)` and treats the
    run as the counterexample.
  - `search_always(k, P)` decides `E G(P)`: the whole run, prefix included, must
    satisfy `P`, so the DFS never enters a state outside it. This is the
    counterexample of `A F(!P)`, used by `check reach`, `check visit` (at least
    once) and `check value --every-run`.
  - `search_eventually_always(k, P)` decides `E F(G(P))`: only the *cycle* has
    to satisfy `P`; the prefix may leave it. This is the counterexample of
    `A G(F(!P))`, used by `check visit --infinitely-often`.
  The `G(F(...))` variant needs the completeness pass (explore the whole
  reachable graph, then look for the cycle), which is why it cannot stop early;
  the `E G(...)` variant must *not* explore through states outside the atom,
  because a run that leaves and comes back is not a counterexample to `G`.
- Long straight-line programs make the graph deep, so both searches are
  iterative (explicit stack, no recursion) - a recursive DFS overflows on a
  200 000-instruction program.
- **An unbounded symbolic input makes the state space infinite**, so a search
  may not terminate; that is the same limitation as before and what
  `--max-stdin-length` and `--input` are for (`spec/input.md`). The searches are
  sound on an infinite graph (a found cycle is a real counterexample) but only
  complete when the relevant part is finite.
- **`check visit --infinitely-often` means "before halting".** The end point
  stutters, so the property is `G(F(!halt & cell))`, not `G(F(cell))`. Excluding
  the halt atom makes every terminating run a counterexample, which is what
  turns a successful run into a nontermination proof. Without the flag,
  `F(cell)` still counts the halting state: a final `>` really does move the
  pointer. Guarded by `check_visit_halting_program_fails_infinitely_often` and
  `check_visit_at_least_once`.
- Labels are a `std::multimap` keyed by instruction pointer, so several labels
  may share one pointer and all of them are kept. `has_label` is a linear scan
  over the map, `has_label_at` a scan of one pointer's equal range.
- `IOModel::read_next_char()` uses `std::cin.get()`, not `std::cin >>`, because
  formatted extraction skips whitespace and would silently drop space, tab and
  newline input characters. It also consumes the `--max-stdin-length` budget, so
  `execute` and `check reach` agree on when the input ends.
- **A pinned input is not part of `KState`.** `IOModel::set_input()` sets
  `chars_until_eof` to the input's length and the budget doubles as the cursor
  (`input[input.size() - remaining]`), so the state identity stays
  `(pc, mem_ptr, memory, remaining)`. Do not add the bytes to the state, and do
  not let `set_chars_until_eof()` desynchronise the two: the CLI already rejects
  `--input` together with `--max-stdin-length`. `decode_input_escapes()` is the
  single decoder for the CLI syntax, used by both the validator and the model.
- `MemoryModel` rejects a tape size of zero (`std::invalid_argument`); the
  `argparse` layer also rejects `--memory-size 0`.

## Pull requests

- Update the roadmap checkboxes in `README.md` when completing an item.
- Keep changes scoped: a feature usually touches
  `brainfuck/{program,model,kripke,search,analysis}`, `argparse`, and
  `tests.cpp`.
- CI must stay green (`cmake .`, `make`, `make test`).
