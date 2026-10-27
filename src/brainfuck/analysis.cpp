#include <string>
#include <optional>
#include <utility>
#include <vector>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/emptiness.hh>
#include "analysis.hpp"
#include "program.hpp"
#include "kripke.hpp"

namespace brainfuck
{
    namespace
    {
        //! Finds a run of \a prog that violates \a property.
        /**
         * Complements \a property, translates it with Spot and asks the
         * program's Kripke structure for an intersecting run. This is the
         * shared engine of every check: a run exists exactly when the property
         * does not hold on every run.
         *
         * \param prog the program whose runs are explored
         * \param property the LTL property that every run should satisfy
         * \param folding which straight-line runs may be folded away
         * \param observation cell pointer, cell content, wrap or halt point to expose
         */
        std::optional<spot::twa_run_ptr> find_counterexample(
            Program prog,
            const spot::formula &property,
            Folding folding,
            Observation observation)
        {
            auto d = spot::make_bdd_dict();
            spot::twa_graph_ptr af =
                spot::translator(d).run(spot::formula::Not(property));
            auto k = std::make_shared<Kripke>(prog, d, folding, observation);
            if (auto run = k->intersecting_run(af))
                return std::optional<spot::twa_run_ptr>{run};
            return std::nullopt;
        }

        //! Turns the run of a `F(atom)` or `G(!atom)` check into its answer.
        /**
         * For \a every_run the run is a counterexample, so the statement holds
         * exactly when none exists; for the existential question the run is the
         * witness, so the statement holds exactly when one exists.
         */
        ValueAnswer run_answer(bool every_run, std::optional<spot::twa_run_ptr> run)
        {
            if (every_run)
                return ValueAnswer{!run.has_value(), std::move(run)};
            return ValueAnswer{run.has_value(), std::move(run)};
        }
    }

    std::optional<spot::twa_run_ptr> check_reach(Program prog, std::string label, bool compress)
    {
        const spot::formula property =
            spot::formula::F(spot::formula::ap(label));
        return find_counterexample(
            std::move(prog),
            property,
            compress ? Folding::straight_line : Folding::none,
            Observation{});
    }

    std::optional<spot::twa_run_ptr> check_visit(
        Program prog, mem_ptr_t cell, VisitKind kind, bool fold_values)
    {
        const spot::formula cell_ap =
            spot::formula::ap(CELL_VISIT_PROPOSITION);
        spot::formula property;
        if (kind == VisitKind::at_least_once)
        {
            property = spot::formula::F(cell_ap);
        }
        else
        {
            // "Infinitely often before halting": the stuttering end point must
            // not count as a visit, so the halt atom is part of the inner
            // formula. A run that halts eventually satisfies halt forever and
            // therefore violates this property.
            const spot::formula halt_ap =
                spot::formula::ap(HALT_PROPOSITION);
            const spot::formula visit = spot::formula::And(
                std::vector<spot::formula>{spot::formula::Not(halt_ap), cell_ap});
            property = spot::formula::G(spot::formula::F(visit));
        }

        // Pointer runs are never folded here: the pointer position is the
        // property, so Folding::straight_line would be unsound.
        Observation observation;
        observation.visited_cell = cell;
        return find_counterexample(
            std::move(prog),
            property,
            fold_values ? Folding::values : Folding::none,
            observation);
    }

    ValueAnswer check_value(Program prog, CellTest test, ValueKind kind)
    {
        spot::formula atom = spot::formula::ap(CELL_VALUE_PROPOSITION);
        if (test.on_halt)
            // A postcondition only looks at the end point, so the halt atom is
            // part of the observed atom itself.
            atom = spot::formula::And(
                std::vector<spot::formula>{spot::formula::ap(HALT_PROPOSITION), atom});

        // Every run must satisfy the atom: `F(atom)`, and a counterexample is a
        // run that never does. Asking whether some run satisfies it means
        // verifying the negation `G(!atom)`; its counterexample is the witness.
        const bool every_run = kind == ValueKind::every_run;
        const spot::formula property =
            every_run ? spot::formula::F(atom)
                      : spot::formula::G(spot::formula::Not(atom));

        // A value check observes a cell's content in every state, so `+`/`-`
        // runs must stay unfolded. A postcondition observes only the end
        // point, so it may fold like a reachability check.
        Observation observation;
        observation.cell_test = test;
        std::optional<spot::twa_run_ptr> run = find_counterexample(
            std::move(prog),
            property,
            test.on_halt ? Folding::straight_line : Folding::pointers,
            observation);

        return run_answer(every_run, std::move(run));
    }

    std::optional<spot::twa_run_ptr> check_overflow(Program prog, WrapTest test)
    {
        const spot::formula wrap = spot::formula::ap(CELL_WRAP_PROPOSITION);

        // A wrap is an event to hunt for: verify that no run ever wraps, and
        // take the counterexample of `G(!wrap)` as the witness. Its last state
        // is the one whose next step wraps the cell.
        const spot::formula property = spot::formula::G(spot::formula::Not(wrap));

        // A wrap is one `+`/`-` instruction, so value runs must stay unfolded; a
        // folded run would hide the single step that wraps. Pointer runs change
        // no cell and may be folded.
        Observation observation;
        observation.wrap_test = test;
        return find_counterexample(
            std::move(prog), property, Folding::pointers, observation);
    }
}
