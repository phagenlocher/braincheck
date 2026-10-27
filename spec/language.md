# Source language and parser

Implemented in `src/brainfuck/program.{hpp,cpp}`.

## Dialect

Brainfuck has no comment syntax. In braincheck every byte of the source is
either one of the eight instructions or ignored, except for labels:

| Character | `Instruction` | Meaning |
| --- | --- | --- |
| `<` | `left` | move the memory pointer left |
| `>` | `right` | move the memory pointer right |
| `+` | `inc` | increment the current cell |
| `-` | `dec` | decrement the current cell |
| `,` | `get` | read one character into the current cell |
| `.` | `put` | write the current cell as one character |
| `[` | `fwd` | jump behind the matching `]` if the current cell is zero |
| `]` | `bwd` | jump behind the matching `[` if the current cell is not zero |

`instr_char()` is the single mapping back to the source character; when you add
an instruction, update the enum, `instr_char()`, the parser, `Program::run()`
and `KIterator` together.

Parsing keeps **every** instruction. The folding of consecutive `+`/`-` and
`>`/`<` into one Kripke transition happens later, in the state space: the
`Program` carries a block table but `instr_count()`, `print`, `execute`, labels
and jump targets describe the unfolded source (see
[state-space.md](state-space.md)).

Everything else (`space`, newline, `#`, letters, other punctuation, ...) is
skipped. That is why the prose in `examples/*.bf` avoids the eight instruction
characters - a stray `.` in a sentence would print and a stray `,` would read
input.

## Labels

A label is written inline as `_name_` and marks the *next* instruction:

- The parser sees `_`, takes everything up to the following `_` as the name, and
  records it at the current instruction count. The name may be empty (`__` is a
  label called `""`).
- **Instruction characters inside a label are not instructions.** `_a+b_` adds
  the label `a+b`, not a `+`.
- An unterminated label runs to the end of the file and is accepted:
  `+_abc` gives the label `abc` at pointer 1. `print` then renders it as
  `+_abc_`, so printing is not byte-identical to the input.
- A label after the last instruction is legal and sits at `pc == instr_count`,
  the program's end point. In the model checker that state stutters, so such a
  label means "the program has halted". `_done_` at the end of a program is a
  reachable state, not decoration (see [state-space.md](state-space.md)).
- Labels are a `std::multimap<instr_ptr_t, std::string>`: several labels may
  share one instruction pointer, and the same *name* may appear at several
  pointers. Every occurrence is kept and every occurrence is marked in the
  Kripke structure (see [state-space.md](state-space.md)).
- Because `_` is the only label delimiter, an underscore in prose creates a
  label. `some_text_here` reads as the label `text`.
- `has_label(name)` is a linear scan over all labels, by name.

## Jumps

The parser pairs brackets on the fly with a stack:

| At | Target |
| --- | --- |
| `[` at `p`, matching `]` at `q` | `jump[p] = q + 1` |
| `]` at `q` | `jump[q] = p + 1` |

Both targets are one past the partner instruction, so a jump out of the last
bracket lands on `pc == instr_count` and ends the run. `Program::jump_target()`
is the accessor and is only valid for a bracket pointer (it uses `at()` and
throws `std::out_of_range` otherwise - that is a bug in the caller, not user
input).

## Errors

The only parse errors are unbalanced brackets and an unreadable file. All of
them throw `brainfuck::ParseException`; `main.cpp` prints
`Parse error: <what()>` and exits with status **1**.

| Condition | Message |
| --- | --- |
| `]` without a preceding `[` | `Jump instructions are not balanced` (thrown at the offending `]`) |
| `[` never closed | `Jump instructions are not balanced` (thrown after the loop) |
| file cannot be opened | `Could not open file` |

Nothing else is rejected: unknown bytes are ignored, an empty file is a valid
(empty) program, labels may be duplicated, and a program is never checked for
termination or for a `,` without input.

## Program object

- `instr_count()` is the number of instructions, not the source size.
- `instr_for_pc(pc)` returns `std::nullopt` for `pc >= instr_count()`, which is
  how the end point is detected; `is_end(pc)` says the same thing explicitly and
  is what the model checker and the counterexample display use.
- `label_for_instr_ptr(pc)` returns one of the labels at `pc`, if any.
- `get_label_map()` exposes the label multimap; the state conditions are built
  from it once per `Kripke`.
- `block_at(pc)` returns the straight-line block that covers `pc`, and
  `block_end(pc)` the pc after it (`block_end(instr_count())` is
  `instr_count()`). The block table has one entry per instruction pointer and is
  built during parsing; see [state-space.md](state-space.md) for the folding
  rules.
- The instruction list, labels, jump table and block table are immutable and
  shared. A `Program` copy is O(1); do not add mutable members to it, and do not
  mutate the tables after parsing (they are `const` behind `shared_ptr`).
- `parse_from_file()`/`parse_from_istream()` take the `MemoryModel` and
  `IOModel` by value and store them in the returned program; parsing reads the
  entire stream once. The `MemoryModel` is also used to build the block table:
  the clamp bounds of pointer blocks depend on the tape size, so fix the tape
  configuration before parsing (the CLI does). Changing it afterwards leaves the
  block table stale, just like `set_cell_size()` does not re-normalise cells.
- `print(without_label)` renders the program from the parsed tables: for every
  pointer from 0 to `instr_count()` it prints the labels there, then the
  instruction (if any), and finishes with a newline. Comments and whitespace of
  the source are gone, and a label at the end is printed.
