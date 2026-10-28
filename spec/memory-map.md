# `MemoryMap`: persistent state memory

Implemented in `src/brainfuck/memory_map.{hpp,cpp}`.

Every `KState` stores its cells in a `MemoryMap`. It is a persistent
(structurally shared) map because every transition produces a new snapshot and
every snapshot stays alive in the search's state table: copying a `std::map` per
successor made exploration quadratic in the number of live cells.

## Representation

- A treap keyed by cell index. Nodes are **immutable**: an update allocates
  O(log n) new nodes along the path and shares every other node with the
  snapshot it came from.
- Copying a `MemoryMap` is O(1) (one `shared_ptr` copy).
- The priority of a cell is `mix_hash(cell)`, so the shape is a deterministic
  function of the key set. Two maps with equal contents therefore have the same
  shape and the same hash, no matter in which order they were built.

## Contents

- A cell holding `0` is **absent**. `set(cell, 0)` removes the cell, and a zero
  value is never stored; `get()` returns `0` for absent cells.
- `set()` with the value a cell already has returns the same snapshot (no
  allocation).
- `size()` is the number of present cells, not the tape size.

## API contract

| Member | Cost | Notes |
| --- | --- | --- |
| `get(cell)` | O(log n) | `0` for an absent cell |
| `set(cell, value)` | O(log n) allocations | returns a new snapshot; the receiver is untouched |
| `hash()` | O(1) | cached per subtree |
| `compare(other)` | O(1) typical, O(n) worst case | strict weak ordering, consistent with equality |
| `size()`, `empty()` | O(1) | cached per node |
| `to_std_map()` | O(n) | debugging/tests only |

`hash()` is the XOR of all entry hashes
`mix_hash(cell ^ mix_hash(value + 0x9e3779b97f4a7c15))`, so it is order
independent and equal contents always produce equal hashes.

`compare()` orders by root identity (same snapshot), then size, then hash, then
the entries in key order. The first two steps keep the entry-wise walk - the
expensive part - off everything but real hash collisions. The returned order is
an implementation detail; only consistency with `operator==` is promised.

## Immutability rules

- **Never move, write or otherwise mutate a child of a node.** Snapshots share
  nodes; stealing a child from a shared node silently changes every earlier
  snapshot. This exact bug was present in `erase()` and made states lose cells.
- The node members are `const`, so a `std::move` of a child degrades into a copy
  instead of stealing it. Keep them `const`; do not add setters.
- All helpers (`insert`, `replace`, `erase`, `merge`, `split`) are pure: they
  take nodes by value and return a new root.
- Zero cells must never be stored, otherwise `hash()` and `compare()` disagree
  with the canonical form and states stop matching.

`memory_map_matches_std_map` and `memory_map_immutability` in `src/tests.cpp`
guard this: after every operation they check both the new snapshot and the
snapshot it was derived from against a `std::map` reference.
