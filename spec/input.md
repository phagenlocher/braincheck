# Standard input model

Implemented in `src/brainfuck/model.{hpp,cpp}` (`IOModel`), consumed by
`Program::run()` (interpreter) and `KIterator` (analysis).

## Settings

| Setting | Default | Meaning |
| --- | --- | --- |
| `input` | none (`std::nullopt`) | pinned input bytes; none means a symbolic input |
| `chars_until_eof` | none (`std::nullopt`) | bytes still readable; none means unbounded |
| `eof_char` | `0` | character delivered after the input ends |
| `no_change_on_eof` | `false` | leave the cell untouched instead of writing `eof_char` |

The CLI exposes them as `--input`, `--max-stdin-length`, `--eof-char` and
`--no-change-on-eof` on `execute`, `check reach` and `check visit` (see
[cli.md](cli.md)).
`--input` and `--max-stdin-length` exclude each other.

## Alphabet

For a symbolic input, a read can deliver **any** of the 256 byte values,
including `0` and the byte `eof_char`. There is no whitespace handling, no line
buffering and no encoding: a byte is a byte. `0` is an ordinary value, which is
why a program can terminate its input loop by reading `0`. (A pinned input is
the exception: it makes the read deterministic, see below.)

The set of possible characters for one read is:

| State of the budget | Possible characters |
| --- | --- |
| symbolic, unbounded (`nullopt`) | all 256 |
| symbolic, `remaining > 0` | all 256 |
| pinned (`input` set), `remaining > 0` | exactly the next byte of the pinned input |
| `remaining == 0`, `no_change_on_eof == false` | exactly `{eof_char}` |
| `remaining == 0`, `no_change_on_eof == true` | none - the cell is left alone |

## Concrete input

`--input BYTES` pins the input to a fixed sequence instead of leaving it
symbolic. Every read then delivers exactly the next byte, so `check reach` and
`check visit` explore one successor per read rather than 256 and answer **for
that input only** (see [reachability.md](reachability.md) and
[cell-visits.md](cell-visits.md)).

- `IOModel::set_input()` also sets `chars_until_eof` to the input's length. The
  budget is the cursor into the pinned input: with `n` bytes still readable, the
  next byte is `input[input.size() - n]`. That is why the bytes themselves are
  not a state field (see [state-space.md](state-space.md)).
- Because the budget *is* the cursor, pinning an input and setting a separate
  length budget contradict each other. The CLI rejects the combination instead
  of picking a winner; the library documents the ordering precondition on
  `set_input()`.
- Once the pinned bytes are consumed, the end-of-input rules above apply
  unchanged, so `--eof-char` and `--no-change-on-eof` keep their meaning.
- A NUL byte inside the input is an ordinary byte, written as `\x00` or `\0`
  because an argument vector cannot carry it. See [cli.md](cli.md) for the
  escape syntax.

## Budget

`chars_until_eof` counts **reads**, not instructions and not bytes consumed from
a stream. Only a `,` decrements it, in both consumers:

- `IOModel::read_next_char()` (interpreter) decrements on a successful read;
- `KIterator` decrements the counter it copied into the successor state, and
  only in the `get` branch.

Do not decrement the budget on other instructions: that was a bug that made
`--max-stdin-length N` behave like "only the first N instructions may read".

## Interpreter path

`IOModel::read_next_char()` returns `std::optional<io_char_t>`:

1. budget is `0` -> `std::nullopt`;
2. a pinned input -> the next byte of that input and the budget is decremented;
   standard input is not read at all;
3. otherwise read one byte with `std::cin.get()`;
4. end of standard input -> `std::nullopt` (the budget is not consumed);
5. otherwise decrement the budget and return the byte.

`std::cin >> c` must never be used here: formatted extraction skips whitespace,
so `space`, `tab` and `newline` would silently disappear from the input.

`Program::run()` then writes the byte with `set_current_value()`, or, on
`std::nullopt`, writes `eof_char` unless `no_change_on_eof` is set, in which
case the cell keeps its value. See [execution.md](execution.md).

## Analysis path

`KIterator` asks `IOModel::get_possible_chars(remaining)` and creates one
successor per returned character, normalising each through
`MemoryModel::normalize_value()` (a 16-bit cell receives the same byte value as
an 8-bit cell). If the returned set is empty (`no_change_on_eof` after the
input ended) it creates exactly one successor that does not change the cell.
With a pinned input the set holds one byte, so the read is deterministic.

`remaining_stdin_chars` is part of the state identity (see
[state-space.md](state-space.md)), so the budget must be threaded through the
successor state, not stored in the `IOModel`. The pinned bytes may live in the
`IOModel` because they never change during one exploration; their cursor may
not.

`IOModel::read_possible_chars()` is the stateful variant used by the tests: it
advances the model while returning the alphabet.

## Consequences

- With unbounded input (`--max-stdin-length` not given) a program with a `,`
  inside a loop has an infinite state space; the analysis may not terminate.
- `--max-stdin-length N` bounds the number of *reads*, so it usually makes such
  a program finite.
- `--input BYTES` removes the input from the state space entirely: every read is
  deterministic, so `check reach` and `check visit` answer for one fixed input
  instead of for every input. A fixed input can still loop forever.
- `check reach` and `check visit` do not read standard input; the model
  describes what *could* be read (or, with `--input`, what is read).
- `execute` reads the real standard input and applies the same budget, so both
  commands agree about when the input ends. With `--input` it consumes the
  pinned bytes instead of standard input.
