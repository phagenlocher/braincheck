# Execution semantics (`execute`)

Implemented in `src/brainfuck/program.cpp` (`Program::run`) on top of
`MemoryModel` and `IOModel`.

`execute` is a plain interpreter for one concrete run with the real standard
input. It does not analyse anything and it never explores alternatives.

## Loop

`run()` resets the memory model (pointer `0`, all cells `0`), then executes
instructions at increasing `ip` until `ip >= instr_count()`:

| Instruction | Effect on the model | Next `ip` |
| --- | --- | --- |
| `<` | `pointer_left()` | `ip + 1` |
| `>` | `pointer_right()` | `ip + 1` |
| `+` | current cell `next_value()` | `ip + 1` |
| `-` | current cell `prev_value()` | `ip + 1` |
| `,` | write the next input byte, or apply the end-of-input rule | `ip + 1` |
| `.` | write the current cell to standard output | `ip + 1` |
| `[` | nothing if non-zero, else jump | `ip + 1` or `jump_target(ip)` |
| `]` | nothing if zero, else jump | `ip + 1` or `jump_target(ip)` |

All jump targets are "one past the matching bracket" (see
[language.md](language.md)), so leaving the last loop lands exactly on
`instr_count()` and the program stops.

## Input

On `,` the interpreter asks `IOModel::read_next_char()`. On end of input:

- `no_change_on_eof == false` (default): the cell becomes `eof_char` (`0` by
  default);
- `no_change_on_eof == true`: the cell keeps its value.

The read consumes one unit of `--max-stdin-length`, exactly like the analysis
does. With `--input` the interpreter consumes the pinned bytes instead of
standard input and never touches the stream. See [input.md](input.md).

## Output

`.` writes **exactly one byte**: `std::cout.put(static_cast<char>(value))`,
i.e. the low eight bits of the cell, regardless of the cell size. With 16- or
32-bit cells a value of 300 prints byte 44. There is no newline added at the end
of a run and no flushing beyond what the stream already does; the output is
therefore the concatenation of the printed cells.

## Termination and errors

- `run()` has no step limit and no recursion: a non-terminating program runs
  until the user stops it. This is the program's problem, not the
  interpreter's.
- At the end point (`pc == instr_count()`) the interpreter simply stops. The
  model checker keeps that state alive with a self-loop instead; both agree on
  where the program ends (see [state-space.md](state-space.md)).
- The exit status is `0` for every run that parses, even if the program never
  terminates normally or prints nothing.
- Parse errors are detected before the run and exit with status `1` (see
  [language.md](language.md)).
- `execute` is deterministic for a given input; `check reach` and `check visit`
  are the commands that consider all possible inputs.
