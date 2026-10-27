# Memory model

Implemented in `src/brainfuck/model.{hpp,cpp}` (`MemoryModel`).

`MemoryModel` is two things at once: the configuration the analysis must obey,
and the live state of the interpreter. It is the **only** source of truth for
cell width, tape width, pointer behaviour and value normalisation. Never
hardcode `30000`, `255`, `256` or a wrap-around: ask the model.

## Types

| Type | Underlying | Used for |
| --- | --- | --- |
| `mem_ptr_t` | `unsigned long` | memory pointer / cell index |
| `memory_size_t` | `unsigned long` | number of cells |
| `cell_value_t` | `uint32_t` | one cell, always the widest cell |
| `io_char_t` | `uint8_t` | one byte read or written |

Do not narrow cell storage to `uint8_t`: it silently disables 16- and 32-bit
cells. Cell values are unsigned and can never be negative.

## Configuration

| Setting | Default | Meaning |
| --- | --- | --- |
| cell size | `EightBit` | 8, 16 or 32 bit cells |
| tape size | `30000` | number of cells, must be >= 1 |
| wrapping | `true` | pointer wraps at the tape edges; `false` clamps |

The defaults are `DEFAULT_CELL_SIZE`, `DEFAULT_MEMORY_SIZE` and
`DEFAULT_WRAPPING` in `model.hpp`; the CLI reads its defaults from a default
constructed model rather than repeating the numbers.

A tape size of zero is rejected with `std::invalid_argument` by both the
constructor and `set_memory_size()`; `--memory-size 0` is rejected by the CLI.

`set_memory_size()` clamps the pointer to `size - 1` when the new size would
leave it outside the tape.

`set_cell_size()` does **not** re-normalise cells that are already stored, so a
value written under a wider cell size can exceed the new maximum until it is
written again. (The CLI sets the cell size before the program runs, so this only
matters to library users.)

## Cell values

`get_max_value()` is the mask of the cell size: `0xFF`, `0xFFFF` or `0xFFFFFFFF`.

| Helper | Result |
| --- | --- |
| `normalize_value(v)` | `v & get_max_value()` - a modulo, because cell sizes are powers of two |
| `next_value(v)` | `v + 1`, or `0` when `v == get_max_value()` |
| `prev_value(v)` | `v - 1`, or `get_max_value()` when `v == 0` |
| `shift_value(v, d)` | `v + d` modulo the cell width; `d` is the net delta of a folded `+`/`-` block |

Cell overflow always wraps at the cell width; the `wrapping` flag is about the
**pointer**, not about values. Do not compute `get_max_value() + 1` in
`cell_value_t`: it wraps for 32-bit cells, which is why `normalize_value()`
masks instead. `shift_value()` must stay equivalent to applying
`next_value()`/`prev_value()` `|d|` times, because the interpreter and the
folded model checker have to agree on a whole `+`/`-` run.

`set_value()`/`set_current_value()` normalise on write. `get_value()` returns
`0` for a cell that was never written.

## Pointer

| Helper | Input | Result |
| --- | --- | --- |
| `pointer_left(p)` | `p == 0` | `size - 1` if wrapping, else `0` |
| `pointer_left(p)` | `p > 0` | `p - 1` |
| `pointer_right(p)` | `p == size - 1` | `0` if wrapping, else `size - 1` |
| `pointer_right(p)` | `p < size - 1` | `p + 1` |

The tape is bounded. With wrapping off, moving past an edge silently stays on
the edge cell; it is not an error and does not extend the tape.

`move_pointer(p, net, low, high)` applies a whole folded `>`/`<` block. On a
wrapping tape the result is `(p + net) mod size`. On a clamping tape it is
**not** `pointer_right/left` applied `net` times: the run loses moves that hit
an edge, so it is the composed saturated function `clamp(p + net, low, high)`,
where `low` and `high` are the block's result from cell `0` and from the last
cell. They are computed once per block while parsing, never per state.

`left()`, `right()`, `increment()` and `decrement()` are the stateful wrappers
used by the interpreter; `pointer_left()`, `pointer_right()`, `next_value()`,
`prev_value()`, `normalize_value()`, `shift_value()` and `move_pointer()` are
pure and are shared by the interpreter and the model checker. Keep both paths on
the same helpers.

## Interpreter memory

The interpreter (`Program::run`) keeps its cells in a sparse
`std::map<mem_ptr_t, cell_value_t>`:

- absent cell = `0`;
- explicit zero cells are allowed (the map is not canonicalised);
- `reset()` sets the pointer to `0` and clears all cells, and `run()` calls it
  before executing;
- the map is copied together with the model. That is cheap when it is empty,
  which is the normal case because parsing does not execute anything; it only
  grows after `run()`.

This memory is **not** part of any state identity. The model checker stores
cells in a [`MemoryMap`](memory-map.md) inside each `KState` instead, and only
uses the pure helpers of the `MemoryModel` it was given.
