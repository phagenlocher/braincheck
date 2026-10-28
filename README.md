# braincheck

[![CMake Build and Test](https://github.com/phagenlocher/braincheck/actions/workflows/cmake-build-and-test.yml/badge.svg)](https://github.com/phagenlocher/braincheck/actions/workflows/cmake-build-and-test.yml)
[![License: GPL v3](https://img.shields.io/github/license/phagenlocher/braincheck)](LICENSE)

braincheck is a static and dynamic analyzer for the esoteric programming
language [Brainfuck](https://en.wikipedia.org/wiki/Brainfuck). It parses a
program, builds its state graph, and answers questions about **every** possible
run instead of just the one you happen to execute:

- **Is a label always reached?** — [`check reach`](#check-reach-file-label)
- **Is a cell always visited, once or infinitely often?** —
  [`check visit`](#check-visit-file-cell---infinitely-often)
- **Does a cell reach a particular value?** —
  [`check value`](#check-value-file-cell-op-value---every-run---on-halt)
- **Does a cell wrap past its range?** —
  [`check overflow`](#check-overflow-file-cell---direction-overflowunderflow)
- **What does the program do when it runs?** — `execute`

The project is pre-alpha. Label reachability, cell visitation, cell values and
overflow/underflow work today.

![braincheck in action](https://github.com/phagenlocher/braincheck/assets/15182576/513ba1a2-6f85-45b0-af90-d338b70f604f)

*A reachability query with a counterexample run: blue is the path into the loop,
red the repeating cycle, yellow the label that was asked for.*

## Quick start

### 1. Install the build tools

```sh
# Debian / Ubuntu
sudo apt install cmake g++

# Fedora
sudo dnf install cmake gcc-c++
```

You need CMake 3.27.1 or newer and a C++17 compiler. There is no prebuilt
package and no `make install` target; braincheck is built from source.

### 2. Clone with submodules

```sh
git clone --recursive https://github.com/phagenlocher/braincheck.git
cd braincheck
```

If you cloned without `--recursive`, fetch the submodules afterwards:

```sh
git submodule update --init --recursive
```

### 3. Build

```sh
cmake .
make
```

The build is in-source, so the `braincheck` executable ends up next to
`CMakeLists.txt`. It takes seconds: the only submodules are CLI11 and minunit,
both header-only, and there is no bundled library to compile.

### 4. Verify

```sh
./braincheck --version
make test
```

Optionally, put the binary on your `PATH`:

```sh
sudo install -m 755 ./braincheck /usr/local/bin/braincheck
```

## Try it

The programs in [`examples/`](examples/README.md) are small and labelled, so
you can see both a success and a counterexample:

```sh
# interpret a program once
./braincheck execute examples/hello_world.bf

# a label that every run reaches
./braincheck check reach examples/countdown.bf done

# a label that no run reaches
./braincheck check reach examples/infinite_loop.bf never

# a cell visited on every iteration of an endless loop:
# this also proves that the program never halts
./braincheck check visit examples/cell_visit.bf 1 --infinitely-often

# a witness for the value 42 in cell 1
./braincheck check value examples/cell_value.bf 1 '=' 42

# a cell that wraps 255 + 1 back to 0
./braincheck check overflow examples/cell_overflow.bf 0 --direction overflow
```

More worked examples, including programs whose termination depends on stdin,
the tape size or the cell width, are in
[`examples/README.md`](examples/README.md).

## Labels and checks

Labels are written inline as `_name_` and mark the instruction pointer that
follows them:

```text
_start_++_count_[>+<-]>_spin_+[]
```

A label behind the last instruction means "the program has halted". Put the
label there and `check reach` becomes a termination check: "always reached"
then means that every run halts.

### `check reach FILE LABEL`

Does every run eventually visit `LABEL`? "Every run" includes finite runs that
halt, so a program that stops without visiting the label is a counterexample. If
one exists, it is printed as a lasso:

- **blue** — the path from the start to the loop,
- **red** — the cycle that repeats forever,
- **yellow** — the label you asked about.

### `check visit FILE CELL [--infinitely-often]`

Does every run point at `CELL` at least once? Unlike `check reach`, this check
observes the memory pointer, so `>>>` really visits the cells in between. Cells
are 0-based and the pointer starts on cell 0, so cell 0 is always visited at
least once.

With `--infinitely-often` the question becomes "is `CELL` visited infinitely
often **before halting**?" A successful answer proves that the program never
halts, as long as you picked a cell that every iteration of the suspected loop
touches. That is the closest thing to a nontermination check in braincheck
today.

### `check value FILE CELL OP VALUE [--every-run] [--on-halt]`

Can some run reach a state in which `CELL` holds a value that satisfies
`OP VALUE`? `OP` is `=`, `>`, `>=`, `<` or `<=`, so quote the comparisons:

```sh
./braincheck check value examples/cell_value.bf 1 '>=' 42
```

The cells are checked after every instruction, so a `+++++` run really passes
through 1, 2, 3 and 4 - unlike `check reach`, this check does not fold `+`/`-`
runs. The initial state counts, and so does the halted state: cell 0 is zero
before the program starts, and a program that halts with the value does reach
it.

By default the check is existential: it prints a witness that stops at the
first state where the comparison holds, or reports that no run reaches it. Two
flags change the question:

- `--every-run` asks whether **every** run reaches the value and prints a
  counterexample instead of hunting for one run that does.
- `--on-halt` observes only the state in which the program halts. The default
  then becomes "can some run halt with `CELL` holding ..."; together with
  `--every-run` it becomes the postcondition "every run halts with ...", which
  also fails when a run never halts. This mode only looks at the end point, so
  it folds `+`/`-` runs again.

`VALUE` may exceed the cell width: an 8-bit cell is always `<= 300` and can
never be `300`.

### `check overflow FILE CELL [--direction overflow|underflow]`

Does every run stay inside `CELL`'s range? Holding the maximum is not an
overflow; the overflow is the `+` that crosses it:

- **overflow** — `+` on a cell holding the maximum, say 255 on 8-bit cells;
- **underflow** — `-` on a cell holding zero.

```sh
# cell 0 underflows to 255 and then overflows back to 0
./braincheck check overflow examples/cell_overflow.bf 0

# cell 1 only ever underflows
./braincheck check overflow examples/cell_overflow.bf 1 --direction underflow
```

The check is a safety property: it verifies that **no** run wraps the cell, so
the green answer is `Cell N never wraps on any run.` When some run does wrap it,
that run is printed as a counterexample, truncated on the wrapping
instruction. Without `--direction` both directions count, and the marker says
which one was found (`overflows (255 + 1 -> 0)` or `underflows (0 - 1 ->
255)`). `--direction overflow` or `--direction underflow` narrows the statement
to one of them.

The maximum is the cell width's (`--cell-size`), and wrapping is unrelated to
`--no-wrapping`, which only affects the pointer. A `,` never wraps a cell: it
stores one byte, which is in range for every width, but it can leave the cell at
the maximum so that the next `+` wraps it.

All checks answer over the modelled program — tape size, cell width, pointer
wrapping and input — not over an abstraction. None of them has a built-in
timeout: a loop around `,` with unbounded input usually has an infinite state
space, so bound it with `--max-stdin-length` or pin it with `--input`.

## Commands

| Command | What it does |
| --- | --- |
| `braincheck execute FILE` | Interpret the program once with the real standard input. |
| `braincheck print FILE` | Re-print the parsed program, including its labels. |
| `braincheck dot FILE` | Print the state graph as a Graphviz graph. |
| `braincheck check reach FILE LABEL` | Is the label reached on every run? |
| `braincheck check visit FILE CELL` | Is the cell visited at least once on every run? |
| `braincheck check value FILE CELL OP VALUE` | Can some run reach a value satisfying `OP VALUE`? |
| `braincheck check overflow FILE CELL` | Does no run wrap the cell past its maximum or zero? |

`--help` works on every command (`braincheck check --help`,
`braincheck check visit --help`, and so on), and `--version` prints the program
and CLI11 versions.

## Options

The memory and input models decide what the program can do. `execute`,
`check reach`, `check visit`, `check value` and `check overflow` share these
options:

| Option | Default | Meaning |
| --- | --- | --- |
| `--memory-size N` | `30000` | Number of cells on the tape. |
| `--cell-size 8\|16\|32` | `8` | Cell width in bits; values wrap at this width. |
| `--no-wrapping` | wrapping on | Stop at the tape edges instead of moving to the other end. |
| `--input BYTES` | all 256 bytes | Pin what each `,` reads instead of exploring every byte. |
| `--max-stdin-length N` | unbounded | Maximum number of bytes that can be read. |
| `--eof-char N` | `0` | Byte delivered after the input ends. |
| `--no-change-on-eof` | off | Leave the cell unchanged after the input ends. |

`--input` accepts the escapes `\\`, `\n`, `\t`, `\r`, `\0` and `\xNN`, for
example `--input '\x00OK\n'`. It excludes `--max-stdin-length`, because a pinned
input already has a length. `print` also takes `--no-labels`, and `dot` takes
`--no-compress` to show every instruction as its own state. `check value` adds
`--every-run` and `--on-halt`, `check overflow` adds
`--direction both|overflow|underflow` (it verifies that no run wraps the cell),
`check visit` adds `--infinitely-often`.

The full reference is in [`spec/cli.md`](spec/cli.md).

## How it works

### 1. Program → state graph

The source is parsed into an immutable instruction list. Straight-line `+`/`-`
and `>`/`<` runs are folded into single transitions when the checked property
does not observe them, and a state graph is built whose states are
*(instruction pointer, memory pointer, memory contents, remaining input)*.
Memory is a persistent map, so successors share almost all of their parent's
cells.

A **run** is an infinite path through that graph. A halting program stutters in
its end point (the state behind the last instruction), so it also has exactly
one infinite run: the finite execution followed by the halted state forever.
That is what makes statements about "every run" include terminating programs.

### 2. Every check is an LTL formula

`F p` means "eventually `p`", `G p` means "always `p`", and `A`/`E` quantify
over all runs:

| command | property | meaning |
| --- | --- | --- |
| `check reach L` | `A F(L)` | every run eventually carries label `L` |
| `check visit C` | `A F(cell C)` | every run points at cell `C` at least once |
| `check visit C --infinitely-often` | `A G(F(!halt & cell C))` | every run points at `C` forever, before halting |
| `check value C OP V` | `E F(C OP V)` | some run reaches a state where cell `C` satisfies the test |
| `check value … --every-run` | `A F(C OP V)` | every run reaches such a state |
| `check value … --on-halt` | `E F(halt & C OP V)` | some run halts with the test true |
| `check value … --on-halt --every-run` | `A F(halt & C OP V)` | every run halts with the test true (a postcondition) |
| `check overflow C` | `A G(!wrap(C))` | no run ever executes the `+`/`-` that wraps cell `C` |

### 3. The graph search, and why it decides the formula

There is no temporal-logic engine. Three graph questions cover all eight rows,
and each has a standard equivalence:

- **`E F(P)` = some reachable state satisfies `P`.** If one does, the path to it
  is a finite witness, and it extends to an infinite run because every state has
  a successor (a read has at least one possible byte, the end point stutters).
  If none does, no run can reach `P`. A breadth-first search decides it and
  returns the **shortest witness**. Its dual `A G(!P)` is decided by the same
  search: no hit means no run ever satisfies `P`, which is the green answer of
  `check overflow`, and a hit is the counterexample.
- **`A F(P)`, i.e. `!E G(!P)` = some run stays in `!P` forever.** In a finite
  graph an infinite path contains a cycle, and a cycle can be pumped into an
  infinite path, so such a run exists exactly when the `!P` subgraph has a cycle
  reachable **through `!P` states only** - the prefix is part of the
  requirement. A depth-first search for a back edge returns that lasso.
- **`A G(F(P))`, i.e. `!E F(G(!P))` = some run eventually leaves `P` for good.**
  Here only the *suffix* must stay in `!P`; the prefix may pass through `P`.
  The search therefore explores the whole reachable graph first and then looks
  for a cycle inside the `!P` states.

The last two look alike and are not: using `E G(!P)` for
`check visit --infinitely-often` would reject a run that visits the cell once
before leaving it, while using `E F(G(!P))` for `check reach` would accept a
counterexample that visits the label first. The three routines live in
`src/brainfuck/search.cpp`, each with the formula it decides in its contract.

### 4. What `P` is

`P` is evaluated on one state by `Kripke::observes()`: the pointer position
(`check visit`), the cell content compared with `OP VALUE` (`check value`), or
"the step leaving this state wraps the cell" (`check overflow`). A label check
looks the name up in the label map instead. Two details matter:

- **The wrap is a transition property, so it is evaluated in the state *before*
  the wrapping instruction.** Holding the maximum is not an overflow; the
  overflow is the `+` that crosses it. The successor only shows the wrapped
  value and cannot say which step produced it.
- **The end point counts** unless `--on-halt` excludes it. A halting program
  stutters there, so `G(F(!halt & cell))` fails for every terminating run -
  which is what makes a successful `--infinitely-often` check a proof of
  nontermination.

### 5. Folding never hides what `P` observes

Folding keeps the graph small, so it may only remove states that the checked
property cannot see. `check reach` and `dot` fold both `+`/`-` and `>`/`<`
runs; `check visit` folds `+`/`-` only, because the pointer is observable
(`>>>` would hide cells 1 and 2); `check value` and `check overflow` fold
`>`/`<` only, because a cell's content and the wrapping step are observable
(`+++++` would hide every intermediate value and the one increment that wraps),
and `check value --on-halt` folds everything because it observes only the end
point. Blocks never cross a label.

A counterexample is therefore a real run of the modelled program, not a
heuristic: every printed step is a transition of the graph, and the searches
never merge states reached by different paths.

The exact rules live in [`spec/`](spec/README.md) and are normative for
contributors: most of them fail silently when broken.

## Troubleshooting

- **Submodules are missing.** Run
  `git submodule update --init --recursive`. If an older clone still fetches
  over SSH, `git submodule sync --recursive` updates the URLs from
  `.gitmodules`.
- **An analysis does not finish.** It may be exploring an infinite state space.
  Stop it with Ctrl-C and bound the input with `--max-stdin-length` or
  `--input`.

## Contributing

Contributions are welcome. Before changing behaviour, read the specification in
[`spec/`](spec/README.md); it is normative and most of its rules fail silently.
Build, test and style conventions, plus the repository layout, are in
[`AGENTS.md`](AGENTS.md).

```sh
make test
```

CI runs `cmake .`, `make` and `make test` on every pull request. Please keep it
green, and tick the roadmap below when you complete an item.

## Credits

braincheck vendors two header-only libraries as git submodules:

- [**CLI11**](https://github.com/CLIUtils/CLI11) — command-line parsing
  (BSD-3-Clause).
- [**minunit**](https://github.com/siu/minunit) — the minimal test framework
  (MIT).

The analyses are a direct search over the program's state graph, so braincheck
has no model-checking dependency.

Many thanks to their authors and maintainers.

## Roadmap

- [ ] Better test coverage
- [x] Fold straight-line instruction runs into single states
- [x] Pin the analysed input (`--input`) instead of exploring every byte
- [ ] Merge equivalent states (bisimulation quotient)
- [x] Clean up `README.md`
- [ ] More assertions
  - [x] Cell values (equality and comparisons, `--every-run`, `--on-halt`)
  - [x] Cell visits
  - [x] Overflow and underflow (`--direction`, counterexample run)
- [ ] Dynamic assertion checking with summary

## License

braincheck is free software under the
[GNU General Public License v3.0](LICENSE).
