#pragma once

#include <functional>
#include <optional>
#include <vector>
#include "kripke.hpp"

namespace brainfuck
{
    //! A run of the program: a prefix and, for a counterexample, a cycle.
    /**
     * A finite witness (`search_eventually()`) fills \c prefix only and leaves
     * \c cycle empty. A lasso fills both: the prefix leads from the initial
     * state to the first cycle state, and the last cycle state has a
     * transition back to the first one. A state stands for a whole folded
     * transition, so the printers expand it to the instructions it covers.
     */
    struct Run
    {
        //! States from the initial state to the cycle entry, or the whole witness.
        std::vector<KState> prefix;
        //! The repeating part of a lasso; empty for a finite witness.
        std::vector<KState> cycle;
    };

    //! Inner atom of a property, as a predicate on one state.
    /**
     * The analyses build it from `Kripke::observes()` (cell pointer, cell
     * content, wrap) or from the label map, optionally negated. Passing it in
     * instead of a formula is what lets one search routine decide several
     * checks (see `spec/state-space.md`).
     */
    using StatePredicate = std::function<bool(const KState &)>;

    //! Decides `E F(P)`: is there a run that reaches a state satisfying \a holds?
    /**
     * LTL: `E F(atom)`, with \a holds being `atom`. `check value` asks it for
     * `cell OP value` and uses the run as a witness. `check overflow` verifies
     * the dual `A G(!atom)` for `the step leaving this state wraps the cell`,
     * where the same run is the counterexample: no run returned means no run
     * wraps the cell.
     *
     * Equivalence: `E F(atom)` holds exactly when a state satisfying `atom` is
     * reachable from the initial state. The path to it is already a finite
     * witness, and it can be extended to an infinite run because every state
     * has at least one successor: a read has at least one possible byte, and
     * the end point stutters. A breadth-first search therefore decides the
     * formula and returns the shortest witness.
     *
     * \param kripke the state graph to search; its folding decides which
     *               straight-line runs are states
     * \param holds the atom to reach
     * \return the shortest witness run (prefix only, atom state last), or no
     *         value when no reachable state satisfies \a holds
     * \note the search terminates exactly when the reachable state space is
     *       finite; with unbounded symbolic input it can grow without bound
     *       (see `spec/input.md`)
     */
    std::optional<Run> search_eventually(const Kripke &kripke, const StatePredicate &holds);

    //! Decides `E G(P)`: is there a run whose *every* state satisfies \a holds?
    /**
     * LTL: `E G(atom)`. This is the counterexample side of the universal checks
     * `check reach` (`A F(label)`), `check visit` (`A F(cell)`) and
     * `check value --every-run` (`A F(atom)`). Each of them verifies that no run
     * avoids its atom forever, i.e. `!E G(!atom)`, so they call this routine
     * with the *negated* atom.
     *
     * Equivalence: a run of `G(atom)` is an infinite path inside the atom
     * subgraph that starts at the initial state, so the whole run - the prefix
     * included - has to satisfy the predicate. Every infinite path in a finite
     * subgraph contains a cycle, and a cycle can be pumped into an infinite
     * path, so the formula holds exactly when there is a cycle in the atom
     * subgraph that is reachable *through* atom states only. That is the
     * difference from `search_eventually_always()`, where only the cycle has
     * to satisfy the predicate and the prefix may leave it.
     *
     * \param kripke the state graph to search
     * \param holds the atom a counterexample must stay inside
     * \return a counterexample lasso (prefix + cycle) when some run avoids
     *         \a holds forever, or no value when every run reaches it
     */
    std::optional<Run> search_always(const Kripke &kripke, const StatePredicate &holds);

    //! Decides `E F G(P)`: is there a run that eventually always satisfies \a holds?
    /**
     * LTL: `E F(G(atom))`. This is the counterexample side of
     * `check visit --infinitely-often`, which verifies
     * `A G(F(!halt & cell))`: its negation is `E F(G(halt | !cell))`, so the
     * predicate here is the negated atom `halt | !cell`.
     *
     * Equivalence: `F(G(atom))` holds when some suffix of the run stays inside
     * the atom subgraph, i.e. when an atom state lies on an atom cycle that is
     * reachable from the initial state. The prefix only has to reach that
     * cycle and may pass through states outside the atom set, which is why the
     * whole reachable graph is explored first and the cycle is then searched
     * inside the predicate. That is the difference from `search_always()`,
     * where the prefix is part of the requirement too.
     *
     * `check visit --infinitely-often` also relies on the end point stuttering:
     * the halt states form a self-loop, so a terminating run is always a
     * counterexample and success proves that every run is infinite (see
     * `spec/cell-visits.md`).
     *
     * \param kripke the state graph to search
     * \param holds the atom a counterexample must stay inside forever
     * \return a counterexample lasso when some run leaves \a holds for good, or
     *         no value when every run visits it infinitely often
     */
    std::optional<Run> search_eventually_always(const Kripke &kripke, const StatePredicate &holds);
}
