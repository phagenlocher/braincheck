#pragma once

#include <string>
#include <optional>
#include <spot/twa/twa.hh>
#include "kripke.hpp"
#include "program.hpp"

namespace brainfuck
{
    //! Which cell-visitation property \c check_visit decides.
    enum class VisitKind
    {
        at_least_once,   //!< `F(cell)`: every run points at the cell at least once
        infinitely_often //!< `G(F(cell))` before halting: every run visits it forever
    };

    //! Which quantification a cell-value check answers.
    enum class ValueKind
    {
        every_run, //!< every run satisfies the test at some point
        any_run    //!< some run satisfies the test at some point
    };

    //! Outcome of \c check_value.
    /**
     * \c run is a counterexample for \c ValueKind::every_run and a witness for
     * \c ValueKind::any_run, so the two fields are always consistent: a
     * counterexample exists exactly when the statement does not hold, a
     * witness exactly when it does.
     */
    struct ValueAnswer
    {
        //! Whether the requested statement is true.
        bool holds;
        //! Run to print: counterexample for \c every_run, witness for \c any_run.
        std::optional<spot::twa_run_ptr> run;
    };

    //! Decides whether \a label is reached on every infinite run of \a prog.
    /**
     * Builds the LTL formula `F(label)`, complements it, translates it with
     * Spot and looks for a run of the program's Kripke structure that
     * intersects the complemented automaton (see `spec/reachability.md`).
     *
     * \param prog the parsed program; the IO and memory models inside it
     *             define which runs exist
     * \param label the label to check; it must occur in \a prog
     * \param compress fold straight-line blocks; only tests pass false
     * \return a counterexample run (prefix + cycle) when some run never
     *         reaches \a label, or no value when every infinite run does
     *
     * A halting program still counts as an infinite run because the end point
     * stutters, so a label behind the last instruction is reached by every
     * halting run, while a label that a halting run skipped yields a
     * counterexample.
     */
    std::optional<spot::twa_run_ptr> check_reach(Program prog, std::string label, bool compress = true);

    //! Decides whether every run of \a prog points at \a cell as requested.
    /**
     * The cell proposition is true at a state whose memory pointer is \a cell.
     * Folding is restricted to `+`/`-` runs: a folded pointer run would hide
     * the cells the pointer passes through (see `spec/cell-visits.md`).
     *
     * With \c VisitKind::at_least_once the property is `F(cell)`, and the
     * stuttering end point counts, so a program that moves to \a cell and then
     * halts does visit it. With \c VisitKind::infinitely_often the property is
     * `G(F(!halt & cell))`: the end point does not count, so no halting run
     * can satisfy it and success proves that every run is infinite.
     *
     * \param prog the parsed program; the IO and memory models inside it
     *             define which runs exist
     * \param cell the 0-based tape index to observe
     * \param kind the visitation property to decide
     * \param fold_values fold `+`/`-` runs; only tests pass false
     * \return a counterexample run when some run violates the property, or no
     *         value when every run satisfies it
     *
     * \pre \a cell is below `prog.memory_model.get_memory_size()`; the CLI
     *      rejects an out-of-range cell before calling this function.
     */
    std::optional<spot::twa_run_ptr> check_visit(
        Program prog, mem_ptr_t cell, VisitKind kind, bool fold_values = true);

    //! Decides whether \a test holds on the runs of \a prog.
    /**
     * The observed atom is `cell content OP value`, or `halt & cell content OP
     * value` when `test.on_halt` is set, so a postcondition check only looks at
     * the end point (see `spec/cell-values.md`).
     *
     * `ValueKind::every_run` verifies `F(atom)` and reports a counterexample -
     * a run in which the test never holds. `ValueKind::any_run` verifies
     * `G(!atom)`, whose counterexample is a run that does reach the test, i.e.
     * the witness. The initial state counts, and the stuttering end point
     * counts unless `test.on_halt` restricts the observation to it.
     *
     * Folding follows what the check observes: a value check never folds
     * `+`/`-` runs, because every intermediate value is observable, while a
     * postcondition observes only the end point and folds everything.
     *
     * \param prog the parsed program; the IO and memory models inside it
     *             define which runs exist
     * \param test the cell, comparison, value and halt restriction to observe
     * \param kind the quantification to answer
     * \return whether the statement holds and the run to print, if any
     *
     * \pre `test.cell` is below `prog.memory_model.get_memory_size()`; the CLI
     *      rejects an out-of-range cell before calling this function.
     */
    ValueAnswer check_value(Program prog, CellTest test, ValueKind kind);

    //! Decides whether some run of \a prog wraps \a test's cell.
    /**
     * The observed atom is true in the state whose *next* step is the wrapping
     * `+` or `-`: a state holds it exactly when the transition leaving it
     * wraps the cell (see `spec/cell-overflow.md`).
     *
     * The check is existential. It verifies `G(!wrap)`, whose counterexample is
     * a run that reaches a wrapping step, i.e. the witness. There is no
     * "every run" variant: a wrap is an event to hunt for, not a property every
     * run has to satisfy, and the universal question would answer "no" with a
     * counterexample for almost every program.
     *
     * Folding is `Folding::pointers`: a `+`/`-` run stays unfolded because the
     * wrapping step is one instruction inside it, while a `>`/`<` run changes
     * no cell and may be folded.
     *
     * \param prog the parsed program; the IO and memory models inside it define
     *             which runs exist and how wide a cell is
     * \param test the cell and the direction(s) to observe
     * \return a witness run that stops at a wrapping state, or no value when no
     *         run ever wraps the cell
     *
     * \pre `test.cell` is below `prog.memory_model.get_memory_size()`; the CLI
     *      rejects an out-of-range cell before calling this function.
     */
    std::optional<spot::twa_run_ptr> check_overflow(Program prog, WrapTest test);
}
