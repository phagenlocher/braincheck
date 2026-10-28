#pragma once

#include <optional>
#include <string>
#include "kripke.hpp"
#include "program.hpp"
#include "search.hpp"

namespace brainfuck
{
    //! Which cell-visitation property \c check_visit decides.
    enum class VisitKind
    {
        at_least_once,   //!< `F(cell)`: every run points at the cell at least once
        infinitely_often //!< `G(F(!halt & cell))` before halting: every run visits it forever
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
        std::optional<Run> run;
    };

    //! Decides whether \a label is reached on every infinite run of \a prog.
    /**
     * Verifies `A F(label)` and reports a counterexample when it fails. In the
     * search, `A F(label)` is decided as `!E G(!label)`: a run that never
     * carries the label is a cycle inside the states without it, reachable
     * through states without it, which is exactly what
     * `search_always(prog, !label)` looks for (see `search.hpp` for the
     * equivalence and `spec/reachability.md` for the semantics).
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
    std::optional<Run> check_reach(Program prog, std::string label, bool compress = true);

    //! Decides whether every run of \a prog points at \a cell as requested.
    /**
     * With \c VisitKind::at_least_once the property is `A F(cell)`, decided as
     * `!E G(!cell)` by `search_always(prog, mem_ptr != cell)`. The stuttering
     * end point counts as a visit there, so a program that moves to \a cell and
     * then halts does visit it.
     *
     * With \c VisitKind::infinitely_often the property is
     * `A G(F(!halt & cell))`, decided as `!E F(G(halt | !cell))` by
     * `search_eventually_always(prog, halt | mem_ptr != cell)`. The prefix may
     * visit the cell; the suffix may not, and the halt literal is what makes
     * every terminating run a counterexample - so a successful run is a proof
     * that the program never halts.
     *
     * Folding is restricted to `+`/`-` runs: a folded pointer run would hide
     * the cells the pointer passes through (see `spec/cell-visits.md`).
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
    std::optional<Run> check_visit(
        Program prog, mem_ptr_t cell, VisitKind kind, bool fold_values = true);

    //! Decides whether \a test holds on the runs of \a prog.
    /**
     * The observed atom is `cell content OP value`, or `halt & cell content OP
     * value` when `test.on_halt` is set, so a postcondition check only looks at
     * the end point (see `spec/cell-values.md`).
     *
     * `ValueKind::every_run` verifies `A F(atom)` - "every run eventually
     * satisfies the test" - and decides it as `!E G(!atom)` with
     * `search_always(prog, !atom)`, whose lasso is the counterexample.
     * `ValueKind::any_run` verifies `E F(atom)` with
     * `search_eventually(prog, atom)`, whose shortest path is the witness;
     * equivalently it refutes `A G(!atom)`. The initial state counts, and the
     * stuttering end point counts unless `test.on_halt` restricts the
     * observation to it.
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

    //! Decides whether \a prog never wraps \a test's cell on any run.
    /**
     * Verifies `A G(!wrap)`: no run ever executes the `+` that passes the cell
     * width's maximum or the `-` that passes zero. A wrap is a defect, so the
     * answer we want is "never"; a run that reaches a wrapping step is the
     * counterexample and is printed (see `spec/cell-overflow.md`).
     *
     * The observed atom is true in the state whose *next* step is the wrapping
     * `+` or `-`: a state holds it exactly when the transition leaving it wraps
     * the cell. Its complement is `E F(wrap)`, so the statement is decided by
     * `search_eventually(prog, wrap)` - no run returned means no run wraps the
     * cell. There is no "every run wraps" mode: that is not a question anyone
     * asks, and its counterexample would be almost every run.
     *
     * Folding is `Folding::pointers`: a `+`/`-` run stays unfolded because the
     * wrapping step is one instruction inside it, while a `>`/`<` run changes
     * no cell and may be folded.
     *
     * \param prog the parsed program; the IO and memory models inside it define
     *             which runs exist and how wide a cell is
     * \param test the cell and the direction(s) to observe
     * \return a counterexample run that stops at a wrapping state, or no value
     *         when no run ever wraps the cell (i.e. the statement holds)
     *
     * \pre `test.cell` is below `prog.memory_model.get_memory_size()`; the CLI
     *      rejects an out-of-range cell before calling this function.
     */
    std::optional<Run> check_overflow(Program prog, WrapTest test);
}
