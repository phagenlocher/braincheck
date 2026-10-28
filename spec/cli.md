# Command line interface

Implemented in `src/argparse/argparse.cpp`; the work is delegated to the
callbacks in `src/main.cpp`.

## Commands

| Command | Arguments | What it does |
| --- | --- | --- |
| *(none)* | | print the top level help, exit `0` |
| `--version`, `-v` | | print `braincheck pre alpha` and the CLI11 version, exit `0` |
| `execute` | `filepath` | interpret the program once with the real stdin ([execution.md](execution.md)) |
| `print` | `filepath` | re-render the parsed program, one line ([language.md](language.md)) |
| `dot` | `filepath` | print the state graph as a Graphviz digraph |
| `check` | | command group for analysis checks; bare `check` prints its own help, exit `0` |
| `check reach` | `filepath` `label` | reachability analysis ([reachability.md](reachability.md)) |
| `check visit` | `filepath` `cell` | cell visitation; `--infinitely-often` selects the before-halting GF check ([cell-visits.md](cell-visits.md)) |
| `check value` | `filepath` `cell` `op` `value` | can some run reach a cell content satisfying `op` `value`; `--every-run` asks for all runs, `--on-halt` for a postcondition ([cell-values.md](cell-values.md)) |
| `check overflow` | `filepath` `cell` | does no run wrap the cell past the maximum or zero; `--direction` narrows the wrap, a counterexample is printed ([cell-overflow.md](cell-overflow.md)) |

Each subcommand also accepts `-h`/`--help`. At most one subcommand may be given
at each level. The old top-level `check_reach` spelling has been removed; use
`check reach`.

## Options per command

| Option | `execute` | `print` | `dot` | `check reach` | `check visit` | `check value` | `check overflow` |
| --- | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| `--memory-size N` | yes | | yes | yes | yes | yes | yes |
| `--cell-size 8\|16\|32` | yes | | yes | yes | yes | yes | yes |
| `--no-wrapping` | yes | | yes | yes | yes | yes | yes |
| `--input BYTES` | yes | | | yes | yes | yes | yes |
| `--max-stdin-length N` | yes | | | yes | yes | yes | yes |
| `--eof-char N` | yes | | | yes | yes | yes | yes |
| `--no-change-on-eof` | yes | | | yes | yes | yes | yes |
| `--no-labels` | | yes | | | | | |
| `--no-compress` | | | yes | | | | |
| `--infinitely-often` | | | | | yes | | |
| `--every-run` | | | | | | yes | |
| `--on-halt` | | | | | | yes | |
| `--direction both\|overflow\|underflow` | | | | | | | yes |

`dot` takes the memory options but **no** IO options: it always analyses with the
default (unbounded) input model, so a `,` produces 256 successors there.
`print` ignores all models because it only re-renders the parse result.

`--input` and `--max-stdin-length` **exclude** each other: a pinned input already
fixes how many bytes are readable, and two lengths for one input would be
ambiguous (see [input.md](input.md)). CLI11 reports the conflict with exit `108`.

`dot` folds consecutive `+`/`-` and `>`/`<` instructions into one state by
default. `--no-compress` disables that and prints one state per instruction, for
debugging the raw structure (see [state-space.md](state-space.md)).
`check reach` always uses `Folding::straight_line`; `check visit` always uses
`Folding::values`, which folds `+`/`-` runs but keeps every pointer move as its
own state; `check value` and `check overflow` always use `Folding::pointers`,
which is the mirror image - pointer runs are folded and every `+`/`-` keeps its
own state - unless `--on-halt` is given to `check value`, where only the end
point is observed and everything may be folded. No check has a folding flag: the
choice is part of what the property observes. The flag exists only on `dot`.

`check visit CELL`, `check value CELL` and `check overflow CELL` reject a cell
outside the configured tape before analysing and print the valid range to
stderr; no analysis is started. This mirrors an unknown label in `check reach`.

`check value` takes four positionals: `filepath`, `cell`, `op` and `value`. `op`
must be one of `=`, `>`, `>=`, `<` or `<=`; shell quoting is needed for the
comparisons (`./braincheck check value prog.bf 3 '>=' 7`). `value` is an
unsigned 64-bit number and is deliberately **not** range-checked against the
cell width: with a comparison, a value outside the range is meaningful (an
8-bit cell is always `<= 300`), so the check answers it instead of rejecting it
(see [cell-values.md](cell-values.md)). The check is existential by default and
prints a witness; `--every-run` turns it into a universal check.

`check overflow` takes two positionals, `filepath` and `cell`, and a
`--direction` option: `both` (the default) observes `+` past the maximum and
`-` past zero, `overflow` only the former and `underflow` only the latter. It
has no operator or value, because the threshold is the configured cell width
(`--cell-size`) and not a user-supplied number, and no quantification flag: it
verifies `A G(!wrap)` - no run may wrap the cell - and a wrapping run is the
counterexample (see [cell-overflow.md](cell-overflow.md)).

All options default to the models' own defaults (30000 cells, 8-bit cells,
wrapping on, unbounded input, `eof_char = 0`, `no_change_on_eof` off), and an
option that is not given never overrides them. `execute`, `check reach`,
`check visit`, `check value` and `check overflow` register their IO options
separately and bind them to one shared options struct, so a value given to one
command never leaks into the other (only one subcommand runs per invocation
anyway).

Option types as CLI11 sees them:

| Option | Type | Constraint | Failure |
| --- | --- | --- | --- |
| `--memory-size` | unsigned | positive | exit `105` (validator) |
| `--cell-size` | string | one of `8`, `16`, `32` | exit `105` (validator) |
| `--input` | string | decodable escapes | exit `105` (validator) |
| `--max-stdin-length` | unsigned | - | exit `104` on a non-number |
| `--eof-char` | `uint8_t` | `0..255` | exit `104` outside the range |
| `op` (`check value`) | string | one of `=`, `>`, `>=`, `<`, `<=` | exit `105` (validator) |
| `value` (`check value`) | unsigned 64-bit | - | exit `104` on a non-number |
| `--direction` (`check overflow`) | string | one of `both`, `overflow`, `underflow` | exit `105` (validator) |

Because `--memory-size`/`--cell-size` are validated as strings, a non-numeric
`--memory-size` is a validator failure (`105`), while a non-numeric
`--max-stdin-length` is a conversion failure (`104`).

`--eof-char` is a **number**, not a character: `--eof-char 65` delivers `A`.

## `--input` escape syntax

`--input` takes the bytes literally, except for these escapes, each of which
becomes exactly one byte:

| Escape | Byte |
| --- | --- |
| `\\` | backslash |
| `\n` | newline (`10`) |
| `\t` | tab (`9`) |
| `\r` | carriage return (`13`) |
| `\0` | NUL (`0`) |
| `\xNN` | byte `NN`, exactly two hexadecimal digits, either case |

Anything else after a backslash is an error (exit `105`), as is `\x` without
exactly two digits: `\x41bc` is `A` followed by `bc`, not an error. Use `\x00` or
`\0` for a NUL byte, which an argument vector cannot carry; `--input ''` is the
empty input, i.e. the input ends before the first read.

The validator and the model use the same decoder (`decode_input_escapes()` in
`src/brainfuck/model.cpp`), so they cannot disagree about the syntax.

## Streams

| Output | Stream |
| --- | --- |
| program output (`execute`), `print` text, `dot` graph, analysis result | stdout |
| parse errors, missing label, out-of-range cell, CLI11 errors | stderr |

A command never splits one answer across both streams.

## Exit codes

| Code | Cause |
| --- | --- |
| `0` | success, including "label will not be reached", "label does not exist", both `check visit` answers, all `check value` and `check overflow` answers, and a cell outside the tape |
| `1` | parse error or unreadable file (`main.cpp` calls `exit(1)`) |
| `104` | option value cannot be converted to its type (CLI11 conversion error) |
| `105` | option value rejected by a validator (CLI11 validation error) |
| `106` | a required positional or option is missing |
| `108` | two options that exclude each other were both given (CLI11 `ExcludesError`) |
| `109` | unknown option, or a positional that was not expected |

`--help`, `--version`, no subcommand and a bare `check` all exit `0`. A wrong
result is never signalled by a non-zero exit status - parse the text if you need
to tell the two analysis answers apart.

## Notes for the option layer

- `argparse` only parses and dispatches; keep analysis logic in the callbacks.
- Add a shared option by extending the shared option helper
  (`add_memory_options`, `add_io_options`) and the corresponding `make_*_model`
  function, never by printing or analysing inline. A command-specific flag such
  as `--no-labels` or `--no-compress` stays local to its subcommand.
- `--input` is registered by `add_io_options`, so `execute`, `check reach`,
  `check visit`, `check value` and `check overflow` share it, and its validator
  calls the same `decode_input_escapes()` that `make_io_model()` uses. Do not
  decode the argument anywhere else.
- Do not pass pointers or references to the parsed values into the library; the
  models are built by value and handed to the callbacks.
