//! Umbrella header for the `brainfuck` library.
/**
 * `main.cpp` and `tests.cpp` include only this file, so every public type of
 * the parser, the models, the persistent memory, the state graph, the searches
 * and the analyses is reachable through it.
 */
#pragma once

#include "model.hpp"
#include "memory_map.hpp"
#include "program.hpp"
#include "kripke.hpp"
#include "search.hpp"
#include "analysis.hpp"
