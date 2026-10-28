# Examples

Small Brainfuck programs for trying out `braincheck`. Labels are written inline
as `_name_` and mark the instruction pointer at that point.

Brainfuck has no comment syntax: every `+`, `-`, `<`, `>`, `.`, `,`, `[` and `]`
character in a file is an instruction, even inside a sentence. That is why the
explanatory lines in these files avoid all eight characters (a comma in
"Hello, World" would read from stdin, a full stop would print a cell).

| File | What it shows | Suggested command |
| --- | --- | --- |
| `hello_world.bf` | Straight-line output with loops | `./braincheck execute examples/hello_world.bf` |
| `countdown.bf` | A label that is always reached | `./braincheck check reach examples/countdown.bf done` |
| `infinite_loop.bf` | A label that is never reached | `./braincheck check reach examples/infinite_loop.bf never` |
| `echo.bf` | Input-driven loop, bounded by stdin length | `./braincheck check reach examples/echo.bf read --max-stdin-length 2` |
| `key_check.bf` | Halting depends on stdin: only `OK` reaches the label | `./braincheck check reach examples/key_check.bf halt --input OK` |
| `cell_visit.bf` | Finite, infinite and never-visited cells | `./braincheck check visit examples/cell_visit.bf 1 --infinitely-often` |
| `cell_value.bf` | A cell that reaches 42 on every run | `./braincheck check value examples/cell_value.bf 1 '=' 42` |
| `cell_overflow.bf` | A cell that underflows and overflows | `./braincheck check overflow examples/cell_overflow.bf 0` |
| `cell_wrap.bf` | Eight-bit cell wrapping | `./braincheck execute examples/cell_wrap.bf` |
| `tape_wrap.bf` | Halts with a wrapping pointer, hangs when the pointer clamps | `./braincheck execute examples/tape_wrap.bf --memory-size 4` |
| `cell_size.bf` | Halts with 8-bit cells, hangs with 16- or 32-bit cells | `./braincheck execute examples/cell_size.bf --cell-size 8` |

`print` and `dot` work on all of them, for example:

```sh
./braincheck print examples/countdown.bf
./braincheck dot examples/countdown.bf
```

## Cell visits (`check visit`)

`check visit` asks about the memory pointer instead of a label. Without a flag it
asks whether a cell is visited at least once; `--infinitely-often` asks whether
it is visited infinitely often **before halting**.

`cell_visit.bf` counts cell 0 down, moves to cell 1 and then spins in `[]`
forever. The labels `_start_`, `_count_` and `_spin_` mark the three phases.

```sh
# every run visits cell 0 and cell 1 at least once
./braincheck check visit examples/cell_visit.bf 0
./braincheck check visit examples/cell_visit.bf 1

# cell 0 is left behind after the countdown, so it is not visited forever
./braincheck check visit examples/cell_visit.bf 0 --infinitely-often

# cell 1 is visited on every iteration of the endless spin, which also proves
# that the program does not halt
./braincheck check visit examples/cell_visit.bf 1 --infinitely-often

# cell 2 is never touched
./braincheck check visit examples/cell_visit.bf 2
```

The infinitely-often counterexample for cell 0 is an infinite run: the blue
prefix is the countdown and the red cycle is the `+[]` spin. Cell 0 is visited
only finitely often even though the program never halts.

`countdown.bf` is the opposite case. It halts on cell 1, so cell 1 is visited at
least once but its infinitely-often check fails with a `<halt>` cycle:

```sh
./braincheck check visit examples/countdown.bf 1
./braincheck check visit examples/countdown.bf 1 --infinitely-often
```

`infinite_loop.bf` is the minimal version: it never leaves cell 0, so the
infinitely-often check succeeds there.

```sh
./braincheck check visit examples/infinite_loop.bf 0 --infinitely-often
```

The intended nontermination idiom is to pick a cell that every iteration of the
suspected loop visits. A successful infinitely-often check proves that no run
halts. A cell outside the configured tape is rejected instead of analysed
(`--memory-size` changes the valid range).

## Cell values (`check value`)

`check value` compares one cell's content with a value. The operator is a
positional (`=`, `>`, `>=`, `<` or `<=`), so shell quoting is needed for the
comparisons.

`cell_value.bf` multiplies six by seven into cell 1 and prints the result. The
label `_ready_` marks the state where cell 1 holds 42.

```sh
# the default question: does some run reach 42 in cell 1? (witness printed)
./braincheck check value examples/cell_value.bf 1 '=' 42

# ask it of every run instead
./braincheck check value examples/cell_value.bf 1 '=' 42 --every-run

# 42 is also the value the program halts with
./braincheck check value examples/cell_value.bf 1 '=' 42 --on-halt
./braincheck check value examples/cell_value.bf 1 '=' 42 --on-halt --every-run

# cell 1 passes through smaller values on the way ...
./braincheck check value examples/cell_value.bf 1 '<' 42
# ... but it does not halt with one, so this has no witness
./braincheck check value examples/cell_value.bf 1 '<' 42 --on-halt

# a witness for a comparison, stopping at the first match
./braincheck check value examples/cell_value.bf 1 '>=' 42

# nothing ever writes cell 2
./braincheck check value examples/cell_value.bf 2 '=' 1
```

The default is existential: it prints a witness that stops at the first state
where the comparison holds. `--every-run` turns the question into "does every
run reach it?" and prints a counterexample instead. `--on-halt` observes only
the state in which the program halts, so the default becomes "can some run halt
with it?" and `--on-halt --every-run` becomes a postcondition: a program that
never halts fails it.

`countdown.bf` is a second, smaller example: cell 1 ends up holding 5.

```sh
./braincheck check value examples/countdown.bf 1 '=' 5 --on-halt
./braincheck check value examples/countdown.bf 1 '=' 5 --every-run
./braincheck check value examples/countdown.bf 1 '>' 5
```

## Cell overflows (`check overflow`)

`check overflow` verifies that no run wraps a cell: `+` on a cell holding the
maximum (overflow) or `-` on a cell holding zero (underflow). A cell that merely
*holds* 255 or 0 has not wrapped; the wrap is the instruction that crosses the
boundary.

`cell_overflow.bf` wraps cell 0 twice - it underflows from 0 to 255 on the first
`-` and overflows back to 0 on the following `+` - and then underflows cell 1.
Labels `_wrap_` and `_underflow_` mark the two phases.

```sh
# both directions are observed by default; the counterexample says which one
./braincheck check overflow examples/cell_overflow.bf 0

# narrow to the overflow: the counterexample stops on the `+` that goes 255 -> 0
./braincheck check overflow examples/cell_overflow.bf 0 --direction overflow

# cell 0 does underflow too, on the first step
./braincheck check overflow examples/cell_overflow.bf 0 --direction underflow

# cell 1 only ever underflows
./braincheck check overflow examples/cell_overflow.bf 1 --direction underflow
./braincheck check overflow examples/cell_overflow.bf 1 --direction overflow
```

The check verifies that **no** run wraps the cell: when one does, that run is
printed as a counterexample, truncated on the wrapping instruction, and when
none does the answer is the green `Cell N never wraps on any run.` There is no
"every run" mode, because a wrap is a defect to hunt for rather than a property
of all runs.

The threshold is the configured cell width, so `--cell-size 16` moves the
maximum from 255 to 65535 and a single `-` then underflows to 65535.

## A program whose termination depends on stdin (`key_check.bf`)

The program reads two bytes and halts only when they are `O` and `K`. It builds
the character codes 79 and 75 with multiplication loops, subtracts each from the
byte it has just read, and on a mismatch enters an endless loop on a helper
cell. The label `_halt_` sits behind the last instruction, so it means "the
program has halted".

The endless branch never returns to a read, so the state space stays finite even
with the default unbounded input model: no `--max-stdin-length` is needed and
the analysis finishes in under a second.

```sh
# halts; nothing is printed
printf 'OK' | ./braincheck execute examples/key_check.bf

# hangs forever; stop it with Ctrl-C
printf 'NO' | ./braincheck execute examples/key_check.bf

# any input other than OK is a counterexample, so the label is not always reached
./braincheck check reach examples/key_check.bf halt
```

End of input supplies zero by default, so a short input (for example just `O`)
also fails the check and hangs.

Pinning the input with `--input` answers a narrower question: is the label
reached for **these** bytes? Each read then has exactly one successor instead of
256, so the analysis is both stronger news and cheaper.

```sh
# the label is guaranteed to be reached, because OK makes the program halt
./braincheck check reach examples/key_check.bf halt --input OK

# a pinned NO run never halts, so it is reported as a counterexample
./braincheck check reach examples/key_check.bf halt --input NO
```

## Programs whose termination depends on the memory model

`tape_wrap.bf` and `cell_size.bf` are written so that they terminate under one
memory model and run forever under another. The memory model is not part of
the source file: it comes from the defaults in
`brainfuck/model.hpp` (`EightBit` cells, 30000 cells, wrapping pointer) and
from the `--cell-size`, `--memory-size` and `--no-wrapping` flags. The same
program can therefore halt or hang depending only on how it is invoked. Use
Ctrl-C or `timeout` to stop the non-terminating runs.

### Tape wrapping (`tape_wrap.bf`)

The `wrapping` setting of `MemoryModel` decides what a pointer move does at the
edge of the tape: it either continues at the opposite edge or stays put. The
example prepares a four-cell tape with a nonzero cell at index 0, 1 and 3 and a
zero at index 2, then scans to the left with `[<]` until it finds a zero.

- With wrapping (the default) the first `<` moves from cell 0 to cell 3, the
  scan continues through cell 3 and stops at the zero in cell 2. The program
  halts and prints a newline, and the label `_done_` is always reached.
- With `--no-wrapping` the pointer never leaves cell 0, which stays nonzero, so
  the scan loops forever.

```sh
# halts, prints a newline
./braincheck execute examples/tape_wrap.bf --memory-size 4

# proves that the scan leaves the loop
./braincheck check reach examples/tape_wrap.bf done --memory-size 4

# never halts; reports the endless `<]` loop as a counterexample run
./braincheck check reach examples/tape_wrap.bf done --memory-size 4 --no-wrapping

# also never halts; stop it with Ctrl-C
./braincheck execute examples/tape_wrap.bf --memory-size 4 --no-wrapping
```

`--memory-size 4` keeps the tape short, but the example also works on the
default 30000-cell tape: cell 0 wraps to the (empty, hence zero) last cell.

### Cell width (`cell_size.bf`)

Cell values wrap at the width of the cell, so the same increment can yield
different numbers for 8-, 16- and 32-bit cells. The example first builds 256 in
cell 1 with a countdown loop (sixteen additions of sixteen), then enters an
outer loop that adds 512 per round (a countdown from sixteen that adds
thirty-two each time).

- With 8-bit cells 256 wraps to 0. The outer loop is skipped entirely, the
  program halts and prints a newline.
- With 16- or 32-bit cells cell 1 holds 256, and every round of the outer loop
  adds 512. Adding 512 never changes the value modulo 512, and 256 is not 0
  modulo 512, so cell 1 can never become 0: the loop never ends.

The label `_halt_` sits directly after the outer loop, so the reachability
checker can give the same answer for the halting configuration without running
the program at all.

```sh
# halts, prints a newline
./braincheck execute examples/cell_size.bf --cell-size 8

# never halts; stop it with Ctrl-C
./braincheck execute examples/cell_size.bf --cell-size 16
./braincheck execute examples/cell_size.bf --cell-size 32

# 8-bit: the outer loop is skipped, so the label is always reached
./braincheck check reach examples/cell_size.bf halt --cell-size 8
```

The reachability check is only a quick demo at 8 bits. At 16 and 32 bits the
counterexample lasso is very long, so the analysis is not practical; use
`execute` there to watch the program hang.

Note that the pointer-wrapping dimension does not change these runs, because
`cell_size.bf` never moves the pointer to an edge, and `tape_wrap.bf` never
lets a cell leave the small range 0 to 10, so cell wrapping is not involved.
