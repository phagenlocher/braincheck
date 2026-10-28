#include <optional>
#include <cstdint>
#include <functional>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <argparse.hpp>
#include <brainfuck.hpp>

// ANSI escape sequences used for the user-facing output. Each macro is a
// foreground colour; the _BOLD variants use the bright variant of the colour.
// RESET restores the terminal's default attributes.
#define RESET "\x1b[0m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define RED_BOLD "\x1b[31;1m"
#define GREEN_BOLD "\x1b[32;1m"
#define YELLOW_BOLD "\x1b[33;1m"
#define BLUE_BOLD "\x1b[34;1m"
#define MAGENTA_BOLD "\x1b[35;1m"
#define CYAN_BOLD "\x1b[36;1m"
#define WHITE_BOLD "\x1b[37;1m"

namespace ap = argparse;
namespace bf = brainfuck;

//! Parses \a filename under the given models, reporting failures to stderr.
/**
 * On a parse error the message is printed in red and the process exits with
 * status 1, so the callbacks below never see an invalid program.
 */
auto parse_bf_program =
    [](
        std::string filename,
        bf::MemoryModel memory_model = bf::MemoryModel(),
        bf::IOModel io_model = bf::IOModel())
{
    try
    {
        return bf::Program::parse_from_file(filename.c_str(), memory_model, io_model);
    }
    catch (bf::ParseException &pe)
    {
        std::cerr << RED_BOLD;
        std::cerr << "Parse error: " << pe.what() << std::endl;
        std::cerr << "Parsing of " << filename << " failed... Aborting!" << std::endl;
        std::cerr << RESET;
        exit(1);
    }
};

namespace
{
    //! Instruction pointer behind the run step that starts at \a pc.
    /**
     * A printed step covers exactly the instructions the Kripke transition
     * folded into it: a whole value block under \c Folding::values, a whole
     * pointer block under \c Folding::pointers, a whole block under
     * \c Folding::straight_line, and one instruction otherwise.
     */
    bf::instr_ptr_t step_end(const bf::Program &prog, bf::instr_ptr_t pc, bf::Folding folding)
    {
        if (prog.is_end(pc))
            return pc;
        const bf::Block block = prog.block_at(pc);
        switch (folding)
        {
        case bf::Folding::straight_line:
            return block.end;
        case bf::Folding::values:
            return block.kind == bf::BlockKind::values ? block.end : pc + 1;
        case bf::Folding::pointers:
            return block.kind == bf::BlockKind::pointers ? block.end : pc + 1;
        case bf::Folding::none:
            break;
        }
        return pc + 1;
    }

    //! Writes one run step in the program's own syntax, or `<halt>`.
    void print_step(const bf::Program &prog, bf::instr_ptr_t pc, bf::Folding folding)
    {
        if (prog.is_end(pc))
        {
            std::cout << "<halt>";
            return;
        }
        const bf::instr_ptr_t end = step_end(prog, pc, folding);
        for (bf::instr_ptr_t step = pc; step < end; step++)
            std::cout << bf::instr_char(prog.instr_for_pc(step).value());
    }

    //! Prints a counterexample run as prefix, cycle and trailing context.
    /**
     * \param highlight_label label to mark in the trailing context, if any
     *
     * A state represents a whole folded transition, so one step prints all
     * instructions that transition stands for. The trailing context starts
     * behind the last cycle step and is not executed. A counterexample is
     * always a lasso, so \c cycle is not empty here.
     */
    void print_run(const bf::Program &prog, const bf::Run &run, bf::Folding folding,
                   const std::optional<std::string> &highlight_label)
    {
        bf::instr_ptr_t last_ptr = 0;
        std::cout << BLUE_BOLD;
        for (const bf::KState &state : run.prefix)
            print_step(prog, state.get_instr_ptr(), folding);
        std::cout << RED_BOLD;
        for (const bf::KState &state : run.cycle)
        {
            print_step(prog, state.get_instr_ptr(), folding);
            last_ptr = state.get_instr_ptr();
        }
        std::cout << BLUE_BOLD;
        for (bf::instr_ptr_t i = step_end(prog, last_ptr, folding); true; i++)
        {
            const auto label_range = prog.get_label_map().equal_range(i);
            for (auto it = label_range.first; it != label_range.second; ++it)
            {
                const bool highlighted =
                    highlight_label.has_value() && it->second == highlight_label.value();
                if (highlighted)
                    std::cout << YELLOW_BOLD;
                std::cout << bf::LABEL_SEPARATOR << it->second << bf::LABEL_SEPARATOR;
                if (highlighted)
                    std::cout << BLUE_BOLD;
            }

            auto m_instr = prog.instr_for_pc(i);
            if (!m_instr.has_value())
                break;
            std::cout << bf::instr_char(m_instr.value());
        }
    }

    //! Source text of \a op, e.g. `>=`.
    const char *comparison_symbol(bf::Comparison op)
    {
        switch (op)
        {
        case bf::Comparison::eq:
            return "=";
        case bf::Comparison::gt:
            return ">";
        case bf::Comparison::ge:
            return ">=";
        case bf::Comparison::lt:
            return "<";
        case bf::Comparison::le:
            return "<=";
        }
        return "=";
    }

    //! The observed value as a noun phrase: `the value 7` or `a value >= 7`.
    std::string value_text(const bf::CellTest &test)
    {
        if (test.op == bf::Comparison::eq)
            return "the value " + std::to_string(test.value);
        return std::string("a value ") + comparison_symbol(test.op) + " " +
               std::to_string(test.value);
    }

    //! True when \a state is the state a value check is looking for.
    bool value_hit(const bf::Program &prog, const bf::CellTest &test, const bf::KState *state)
    {
        if (test.on_halt && !prog.is_end(state->get_instr_ptr()))
            return false;
        return bf::cell_test_holds(test, state->get_memory_map().get(test.cell));
    }

    //! Marker text for a witness state of a wrap check, or empty when it is not one.
    /**
     * A state holds the wrap atom when the step leaving it is the wrapping
     * increment or decrement, so the direction of the marker comes from the
     * instruction about to run.
     */
    std::string wrap_marker(const bf::Program &prog, const bf::WrapTest &test,
                            const bf::KState *state)
    {
        if (!bf::wrap_test_holds(prog, *state, test))
            return {};
        const bf::cell_value_t max = prog.memory_model.get_max_value();
        const bf::Instruction instr = prog.instr_for_pc(state->get_instr_ptr()).value();
        if (instr == bf::Instruction::inc)
            return "cell " + std::to_string(test.cell) + " overflows (" +
                   std::to_string(max) + " + 1 -> 0)";
        return "cell " + std::to_string(test.cell) + " underflows (0 - 1 -> " +
               std::to_string(max) + ")";
    }

    //! Prints a run up to the first state for which \a marker is non-empty.
    /**
     * Both an existential witness ("some run reaches ...") and an event
     * counterexample ("some run wraps ...") end where the observed condition
     * first holds, so the run is truncated there and the state is marked with
     * the text \a marker returns for it; an empty string means "not that
     * state". The rest of the run is not part of the answer.
     */
    void print_truncated_run(const bf::Program &prog, const bf::Run &run, bf::Folding folding,
                       const std::function<std::string(const bf::KState *)> &marker)
    {
        std::cout << BLUE_BOLD;
        std::string mark;
        const auto show = [&](const bf::KState &state)
        {
            if (!mark.empty())
                return;
            print_step(prog, state.get_instr_ptr(), folding);
            mark = marker(&state);
        };
        for (const bf::KState &state : run.prefix)
            show(state);
        if (mark.empty())
            for (const bf::KState &state : run.cycle)
                show(state);
        std::cout << YELLOW_BOLD;
        std::cout << "  <- " << mark;
        std::cout << RESET;
    }

    //! Escapes \a text for use inside a Graphviz node label.
    std::string dot_escape(const std::string &text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (const char character : text)
        {
            if (character == '"' || character == '\\')
                escaped.push_back('\\');
            escaped.push_back(character);
        }
        return escaped;
    }

    //! Every label written at \a pc as `done, loop`, or `(none)` without one.
    std::string label_text(const bf::Program &prog, bf::instr_ptr_t pc)
    {
        const auto range = prog.get_label_map().equal_range(pc);
        std::string text;
        for (auto entry = range.first; entry != range.second; ++entry)
        {
            if (!text.empty())
                text += ", ";
            text += entry->second;
        }
        return text.empty() ? "(none)" : text;
    }

    //! Prints \a kripke as a Graphviz graph, one node per discovered state.
    /**
     * Breadth-first discovery gives stable node numbers, so the initial state
     * is always node 0 and is entered through the invisible `I` node. The node
     * label keeps the `pc = ...; mp = ...; hash = ...` lines and names every
     * label written at the instruction pointer (`done`, `loop`, or `(none)`),
     * so a node shows the labels that a `check reach` would see there (see
     * `spec/state-space.md`).
     */
    void print_dot(const bf::Kripke &kripke)
    {
        const bf::Program &prog = kripke.get_program();

        std::unordered_map<bf::KState, std::size_t, bf::KStateHash, bf::KStateEqual> index;
        std::vector<bf::KState> states;
        std::vector<std::vector<std::size_t>> edges;

        const bf::KState start = kripke.initial_state();
        index.emplace(start, 0);
        states.push_back(start);
        edges.emplace_back();

        for (std::size_t current = 0; current < states.size(); current++)
        {
            bf::KIterator successors = kripke.successors(states[current]);
            for (successors.first(); !successors.done(); successors.next())
            {
                const bf::KState next = successors.state();
                const auto found = index.find(next);
                std::size_t target;
                if (found == index.end())
                {
                    target = states.size();
                    index.emplace(next, target);
                    states.push_back(next);
                    edges.emplace_back();
                }
                else
                {
                    target = found->second;
                }
                edges[current].push_back(target);
            }
        }

        std::cout << "digraph \"\" {" << std::endl;
        std::cout << "  rankdir=LR" << std::endl;
        std::cout << "  label=\"t\\n[all]\"" << std::endl;
        std::cout << "  labelloc=\"t\"" << std::endl;
        std::cout << "  node [shape=\"box\",style=\"rounded\",width=\"0.5\"]" << std::endl;
        std::cout << "  I [label=\"\", style=invis, width=0]" << std::endl;
        std::cout << "  I -> 0" << std::endl;

        for (std::size_t current = 0; current < states.size(); current++)
        {
            const bf::KState &state = states[current];
            std::cout << "  " << current
                      << " [label=\"pc = " << state.get_instr_ptr()
                      << "; mp = " << state.get_mem_ptr()
                      << "\\nhash = " << state.hash()
                      << "\\n" << dot_escape(label_text(prog, state.get_instr_ptr()))
                      << "\"]" << std::endl;
            for (const std::size_t target : edges[current])
                std::cout << "  " << current << " -> " << target << " [label=\"\"]" << std::endl;
        }
        std::cout << "}" << std::endl;
    }
}

//! `execute`: interpret the program and stream its output to stdout.
ap::ExecuteFun exfun = [](std::string filename, bf::MemoryModel memory_model, bf::IOModel io_model)
{
    auto prog = parse_bf_program(filename, memory_model, io_model);
    prog.run();
};

//! `print`: write the program's instructions and labels to stdout.
ap::PrintFun pfun = [](std::string filename, bool without_label)
{
    auto prog = parse_bf_program(filename);
    prog.print(without_label);
};

//! `dot`: render the state graph of the program as a dot graph.
ap::DotFun dotfun = [](std::string filename, bf::MemoryModel memory_model, bool compress)
{
    bf::Program prog = parse_bf_program(filename, memory_model);
    print_dot(bf::Kripke(prog, compress ? bf::Folding::straight_line : bf::Folding::none));
};

//! `check reach`: report whether \a label is reached on every infinite run.
/**
 * Prints a counterexample run (prefix + cycle) in red when the label can be
 * missed, and a green confirmation otherwise. Both are printed with the
 * program's own syntax: whole folded blocks, `<halt>` at the end point and the
 * trailing source behind the cycle as context, with the checked label
 * highlighted (see `spec/reachability.md`).
 */
ap::ReachFun reachfun = [](std::string filename, std::string label, bf::MemoryModel memory_model, bf::IOModel io_model)
{
    bf::Program prog = parse_bf_program(filename, memory_model, io_model);

    if (!prog.has_label(label))
    {
        std::cerr << RED_BOLD;
        std::cerr
            << "Label \""
            << label
            << "\" does not exist in the specified program."
            << std::endl;
        std::cerr << RESET;
        exit(0);
    }

    auto m_run = bf::check_reach(prog, label);
    if (m_run.has_value())
    {
        auto run = m_run.value();
        std::cout << RED_BOLD;
        std::cout
            << "There exists a run for which the label \""
            << label
            << "\" will not be reached:"
            << std::endl;
        print_run(prog, run, bf::Folding::straight_line, label);
    }
    else
    {
        std::cout << GREEN_BOLD;
        std::cout
            << "Label \""
            << label
            << "\" will always be reached.";
    }
    std::cout << RESET << std::endl;
};

//! `check visit`: report whether every run points at \a cell as requested.
/**
 * Without `--infinitely-often` the check asks for `F(cell)` and counts the
 * halting state, so a program that moves to the cell and halts did visit it.
 * With the flag it asks for `G(F(!halt & cell))`: the halted state does not
 * count, so success also proves that the program never halts.
 *
 * A counterexample run shows the path that misses the cell (blue prefix, red
 * cycle, blue trailing context), printed with one instruction per pointer move
 * because every pointer position is observed (see `spec/cell-visits.md`).
 */
ap::VisitFun visitfun = [](std::string filename, bf::mem_ptr_t cell, bf::VisitKind kind,
                           bf::MemoryModel memory_model, bf::IOModel io_model)
{
    bf::Program prog = parse_bf_program(filename, memory_model, io_model);

    const bf::memory_size_t tape_size = prog.memory_model.get_memory_size();
    if (cell >= tape_size)
    {
        // Mirror the unknown-label behaviour: no analysis, report on stderr,
        // and use status 0 because a bad query is not an analysis answer.
        std::cerr << RED_BOLD;
        std::cerr
            << "Cell " << cell << " does not exist on a tape of "
            << tape_size << " cells (valid: 0.." << (tape_size - 1) << ")."
            << std::endl;
        std::cerr << RESET;
        exit(0);
    }

    auto m_run = bf::check_visit(prog, cell, kind);
    if (m_run.has_value())
    {
        std::cout << RED_BOLD;
        if (kind == bf::VisitKind::at_least_once)
            std::cout
                << "There exists a run for which cell " << cell
                << " will never be visited:"
                << std::endl;
        else
            std::cout
                << "There exists a run for which cell " << cell
                << " will be visited only finitely often before halting:"
                << std::endl;
        print_run(prog, m_run.value(), bf::Folding::values, std::nullopt);
    }
    else
    {
        std::cout << GREEN_BOLD;
        if (kind == bf::VisitKind::at_least_once)
            std::cout << "Cell " << cell << " will be visited on every run.";
        else
            std::cout
                << "Cell " << cell
                << " will be visited infinitely often before halting on every run.";
    }
    std::cout << RESET << std::endl;
};

//! `check value`: report whether a cell's content satisfies a comparison.
/**
 * By default the question is asked of some run: the answer prints a witness
 * that stops at the first state where the comparison holds. With `--every-run`
 * it is asked of every run, and a counterexample is a run in which the
 * comparison never holds.
 *
 * `--on-halt` restricts the observation to the end point, so the check becomes
 * a postcondition: by default "some run halts with ...", with `--every-run`
 * "every run halts with ...". The printed run is folding-aware like the other
 * checks; `--on-halt` folds everything because only the end point is observed
 * (see `spec/cell-values.md`).
 */
ap::ValueFun valuefun = [](
    std::string filename, bf::mem_ptr_t cell, bf::Comparison op, std::uint64_t value,
    bf::ValueKind kind, bool on_halt, bf::MemoryModel memory_model, bf::IOModel io_model)
{
    bf::Program prog = parse_bf_program(filename, memory_model, io_model);

    const bf::memory_size_t tape_size = prog.memory_model.get_memory_size();
    if (cell >= tape_size)
    {
        // Mirror the unknown-label behaviour: no analysis, report on stderr,
        // and use status 0 because a bad query is not an analysis answer.
        std::cerr << RED_BOLD;
        std::cerr
            << "Cell " << cell << " does not exist on a tape of "
            << tape_size << " cells (valid: 0.." << (tape_size - 1) << ")."
            << std::endl;
        std::cerr << RESET;
        exit(0);
    }

    const bf::CellTest test{cell, op, value, on_halt};
    const std::string text = value_text(test);
    const bf::Folding folding =
        on_halt ? bf::Folding::straight_line : bf::Folding::pointers;
    const bf::ValueAnswer answer = bf::check_value(prog, test, kind);

    if (kind == bf::ValueKind::every_run)
    {
        if (answer.run.has_value())
        {
            std::cout << RED_BOLD;
            std::cout << "There exists a run for which cell " << cell << " "
                      << (on_halt ? "does not hold " : "never reaches ") << text
                      << (on_halt ? " when the program halts:" : ":");
            std::cout << std::endl;
            print_run(prog, answer.run.value(), folding, std::nullopt);
        }
        else
        {
            std::cout << GREEN_BOLD;
            std::cout << "Cell " << cell << " will "
                      << (on_halt ? "hold " : "reach ") << text
                      << (on_halt ? " when the program halts on every run."
                                  : " on every run.");
        }
    }
    else if (answer.run.has_value())
    {
        std::cout << GREEN_BOLD;
        std::cout << "Cell " << cell << " can " << (on_halt ? "hold " : "reach ")
                  << text << (on_halt ? " when the program halts:" : " on some run:");
        std::cout << std::endl;
        print_truncated_run(
            prog, answer.run.value(), folding,
            [&prog, &test](const bf::KState *state) -> std::string
            {
                if (!value_hit(prog, test, state))
                    return {};
                return "cell " + std::to_string(test.cell) + " " +
                       comparison_symbol(test.op) + " " + std::to_string(test.value);
            });
    }
    else
    {
        std::cout << RED_BOLD;
        std::cout << "Cell " << cell << " never " << (on_halt ? "holds " : "reaches ")
                  << text
                  << (on_halt ? " when the program halts on any run."
                              : " on any run.");
    }
    std::cout << RESET << std::endl;
};

//! `check overflow`: report whether a cell ever wraps past its range.
/**
 * A wrap is the `+` on a cell holding the cell width's maximum (overflow) or
 * the `-` on a cell holding zero (underflow). The check verifies the safety
 * property `no run ever wraps the cell`: a green answer is the one we want,
 * and a run that wraps is printed as a counterexample, truncated on the
 * wrapping instruction.
 *
 * The observed state is the one *before* the wrapping instruction, because the
 * wrap is a property of the step leaving it. `--direction` narrows the
 * observation to one direction; the default observes both and the counterexample
 * marker says which one was found (see `spec/cell-overflow.md`).
 */
ap::OverflowFun overflowfun = [](
    std::string filename, bf::mem_ptr_t cell, bf::WrapDirection direction,
    bf::MemoryModel memory_model, bf::IOModel io_model)
{
    bf::Program prog = parse_bf_program(filename, memory_model, io_model);

    const bf::memory_size_t tape_size = prog.memory_model.get_memory_size();
    if (cell >= tape_size)
    {
        // Mirror the unknown-label behaviour: no analysis, report on stderr,
        // and use status 0 because a bad query is not an analysis answer.
        std::cerr << RED_BOLD;
        std::cerr
            << "Cell " << cell << " does not exist on a tape of "
            << tape_size << " cells (valid: 0.." << (tape_size - 1) << ")."
            << std::endl;
        std::cerr << RESET;
        exit(0);
    }

    // Base form of the answer's verb: "overflows", "underflows" or "wraps".
    std::string verb = "wrap";
    if (direction == bf::WrapDirection::overflow)
        verb = "overflow";
    else if (direction == bf::WrapDirection::underflow)
        verb = "underflow";
    const bf::WrapTest test{cell, direction};
    const std::optional<bf::Run> counterexample = bf::check_overflow(prog, test);

    if (counterexample.has_value())
    {
        std::cout << RED_BOLD;
        std::cout << "There exists a run in which cell " << cell << " " << verb << "s:";
        std::cout << std::endl;
        print_truncated_run(
            prog, counterexample.value(), bf::Folding::pointers,
            [&prog, &test](const bf::KState *state) -> std::string
            {
                return wrap_marker(prog, test, state);
            });
    }
    else
    {
        std::cout << GREEN_BOLD;
        std::cout << "Cell " << cell << " never " << verb << "s on any run.";
    }
    std::cout << RESET << std::endl;
};

//! Entry point: forwards the command line and the callbacks to argparse.
int main(int argc, char **argv)
{
    return ap::run_with_args(
        argc, argv,
        exfun,
        pfun,
        dotfun,
        reachfun,
        visitfun,
        valuefun,
        overflowfun);
}
