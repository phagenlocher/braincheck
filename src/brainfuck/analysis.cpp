#include <optional>
#include <string>
#include <utility>
#include "analysis.hpp"
#include "kripke.hpp"
#include "program.hpp"
#include "search.hpp"

namespace brainfuck
{
    std::optional<Run> check_reach(Program prog, std::string label, bool compress)
    {
        // Verified: `A F(label)`. Decided as `!E G(!label)`, so the search only
        // walks states that do not carry the label; if the initial state
        // carries it, the formula holds without exploring anything.
        const Kripke kripke(std::move(prog), compress ? Folding::straight_line : Folding::none);
        const Program &program = kripke.get_program();
        return search_always(
            kripke,
            [&program, &label](const KState &state)
            {
                return !program.has_label_at(state.get_instr_ptr(), label);
            });
    }

    std::optional<Run> check_visit(Program prog, mem_ptr_t cell, VisitKind kind, bool fold_values)
    {
        Observation observation;
        observation.visited_cell = cell;
        const Kripke kripke(std::move(prog),
                            fold_values ? Folding::values : Folding::none,
                            observation);
        const Program &program = kripke.get_program();

        if (kind == VisitKind::at_least_once)
            // Verified: `A F(cell)`. Decided as `!E G(!cell)`: a counterexample
            // never points at the cell, the stuttering end point included.
            return search_always(
                kripke,
                [&kripke](const KState &state) { return !kripke.observes(state); });

        // Verified: `A G(F(!halt & cell))`. Decided as
        // `!E F(G(halt | !cell))`: the counterexample eventually leaves the
        // cell for good. The halt literal puts the stuttering end point into
        // the suffix, so every terminating run is a counterexample and success
        // proves that the program never halts.
        return search_eventually_always(
            kripke,
            [&kripke, &program](const KState &state)
            {
                return program.is_end(state.get_instr_ptr()) || !kripke.observes(state);
            });
    }

    ValueAnswer check_value(Program prog, CellTest test, ValueKind kind)
    {
        Observation observation;
        observation.cell_test = test;
        // A value check observes a cell's content in every state, so `+`/`-`
        // runs must stay unfolded; a postcondition observes only the end point
        // and may fold like a reachability check.
        const Kripke kripke(std::move(prog),
                            test.on_halt ? Folding::straight_line : Folding::pointers,
                            observation);

        if (kind == ValueKind::every_run)
        {
            // Verified: `A F(atom)`. Decided as `!E G(!atom)`, so the
            // counterexample stays inside the states where the test fails and
            // the initial state is part of the requirement.
            std::optional<Run> counterexample = search_always(
                kripke,
                [&kripke](const KState &state) { return !kripke.observes(state); });
            return ValueAnswer{!counterexample.has_value(), std::move(counterexample)};
        }

        // Verified: `E F(atom)`. Decided directly by the shortest run to a
        // state that satisfies the test; the same run refutes `A G(!atom)`.
        std::optional<Run> witness = search_eventually(
            kripke,
            [&kripke](const KState &state) { return kripke.observes(state); });
        return ValueAnswer{witness.has_value(), std::move(witness)};
    }

    std::optional<Run> check_overflow(Program prog, WrapTest test)
    {
        Observation observation;
        observation.wrap_test = test;
        // A wrap is one `+`/`-` instruction, so value runs must stay unfolded; a
        // folded run would hide the single step that wraps. Pointer runs change
        // no cell and may be folded.
        const Kripke kripke(std::move(prog), Folding::pointers, observation);

        // Verified: `A G(!wrap)`. Decided as `!E F(wrap)`: the search hunts for
        // a wrapping step, the run it returns is the counterexample, and a
        // search that finds none proves the statement (on a finite state
        // space). The absence of a wrap is the answer we want, so it is the
        // green one in the CLI.
        return search_eventually(
            kripke,
            [&kripke](const KState &state) { return kripke.observes(state); });
    }
}
