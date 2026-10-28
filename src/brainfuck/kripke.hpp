#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "memory_map.hpp"
#include "model.hpp"
#include "program.hpp"

namespace brainfuck
{
    class KState;

    //! Which straight-line runs the state space folds away.
    /**
     * A folded run removes every instruction pointer inside it, so an analysis
     * may only fold a run that it cannot observe (see `spec/state-space.md`).
     * Labels are unobservable inside a block, which is why no block crosses
     * one; the memory pointer is observable to a cell-visit check, which is
     * why pointer runs stay unfolded there, and a cell's content and the
     * wrapping increment or decrement are observable to a value or overflow
     * check, which keeps `+`/`-` runs unfolded there.
     */
    enum class Folding
    {
        none,         //!< one state per instruction
        values,       //!< fold + / - runs only; pointer moves stay observable
        pointers,     //!< fold > / < runs only; cell values stay observable
        straight_line //!< fold + - and > < runs (labels and dot)
    };

    //! Comparison a cell-value check applies to a cell's content.
    enum class Comparison
    {
        eq, //!< `=`
        gt, //!< `>`
        ge, //!< `>=`
        lt, //!< `<`
        le  //!< `<=`
    };

    //! One "cell content COMPARISON value" test observed in a state.
    /**
     * \c value may lie outside the cell's range. A comparison makes such a
     * value meaningful (an 8-bit cell is always `<= 300`), so an unsatisfiable
     * test is answered instead of rejected.
     */
    struct CellTest
    {
        //! Tape index whose content is observed.
        mem_ptr_t cell;
        //! Comparison applied to the content.
        Comparison op;
        //! Right-hand side of the comparison.
        std::uint64_t value;
        //! Observe the test only in the end point ("when the program halts").
        bool on_halt;
    };

    //! Which of the two wraps of a cell a check observes.
    enum class WrapDirection
    {
        both,      //!< `+` past the maximum or `-` past zero
        overflow,  //!< `+` on a cell holding the maximum
        underflow  //!< `-` on a cell holding zero
    };

    //! One "the step leaving this state wraps CELL" test.
    /**
     * A wrap is a transition property, not a state property: a cell only
     * overflows when the `+` that would exceed the cell width is executed. The
     * predicate is therefore evaluated on the state *before* the step, whose
     * instruction pointer names that `+` or `-`, and it must stay false in the
     * state the wrap produces - the successor is indistinguishable from any
     * other cell holding zero or the maximum (see `spec/cell-overflow.md`).
     */
    struct WrapTest
    {
        //! Tape index whose next increment or decrement is observed.
        mem_ptr_t cell;
        //! Which wrap to observe.
        WrapDirection direction;
    };

    //! What a check observes in every state.
    /**
     * A check observes at most one of the three. An empty observation makes
     * `Kripke::observes()` constant false, which is what `check reach` and
     * `dot` rely on: they observe the labels, not a cell.
     */
    struct Observation
    {
        //! Pointer position a visit check observes, or nothing.
        std::optional<mem_ptr_t> visited_cell;
        //! Cell content a value check observes, or nothing.
        std::optional<CellTest> cell_test;
        //! Wrap a check observes, or nothing.
        std::optional<WrapTest> wrap_test;
    };

    //! True when \a content satisfies \a test.
    /**
     * Comparisons promote the cell content to `std::uint64_t`, so a test whose
     * right-hand side exceeds the cell range still answers correctly.
     */
    bool cell_test_holds(const CellTest &test, cell_value_t content);

    //! True when the step leaving \a state wraps \a test's cell.
    /**
     * The observed step is the instruction at the state's instruction pointer,
     * which may be an interior instruction of a value run (see
     * `Folding::pointers`). The cell that changes is the one under the memory
     * pointer, so a step that cannot touch \a test's cell never wraps it.
     *
     * \param prog the program, asked for the instruction and the cell width
     * \param state the state the step leaves
     * \param test the cell and direction to observe
     */
    bool wrap_test_holds(const Program &prog, const KState &state, const WrapTest &test);

    //! One state of the state graph induced by a program.
    /**
     * A state is the instruction pointer, the memory pointer, the memory
     * snapshot and the number of characters still available on stdin, exactly
     * the tuple the analyses search over. The memory is a MemoryMap, so
     * copying a state is O(1).
     *
     * hash() and compare() must agree: both look at every field above, memory
     * contents included. Otherwise the search's hash table stores states that
     * compare equal, the exploration grows without bound and states that are
     * not actually the same are merged (see `spec/state-space.md`).
     */
    class KState
    {
    public:
        //! Initial state of the default configuration: pc 0, cell 0, empty memory, unbounded input.
        KState();
        //! Initial state with \a remaining_stdin_chars characters of input left.
        /**
         * \param remaining_stdin_chars input budget, or no value for unbounded input
         */
        explicit KState(std::optional<std::size_t> remaining_stdin_chars);
        //! State at \a pc with a plain memory snapshot and unbounded input.
        KState(instr_ptr_t pc, mem_ptr_t mem_ptr, const std::map<mem_ptr_t, cell_value_t> &values);
        //! State at \a pc with a plain memory snapshot and an input budget.
        KState(instr_ptr_t pc,
               mem_ptr_t mem_ptr,
               const std::map<mem_ptr_t, cell_value_t> &values,
               std::optional<std::size_t> remaining_stdin_chars);
        //! State at \a pc with an already persistent memory snapshot.
        /**
         * The hash is computed once here, because the search asks for it once
         * per visited state and the fields never change afterwards.
         */
        KState(instr_ptr_t pc,
               mem_ptr_t mem_ptr,
               MemoryMap memory,
               std::optional<std::size_t> remaining_stdin_chars);

        //! Instruction pointer of this state.
        instr_ptr_t get_instr_ptr() const;
        //! Memory pointer of this state.
        mem_ptr_t get_mem_ptr() const;
        //! Reads left before the input ends, or no value for unbounded input.
        std::optional<std::size_t> get_remaining_stdin_chars() const;
        //! Persistent memory snapshot, by const reference; use this per state.
        const MemoryMap &get_memory_map() const;
        //! Plain copy of the memory, for tests and debugging.
        /**
         * Materialises the whole map, so never call this on a hot path.
         */
        std::map<mem_ptr_t, cell_value_t> get_memory() const;

        //! Cached hash over pc, memory pointer, memory contents and input budget.
        std::size_t hash() const;
        //! Total order over exactly the fields that \c hash() covers.
        /**
         * An unbounded input sorts before every bounded one - exactly the
         * order the hash mixes in (`remaining + 1`) - and the memory is
         * compared last through \c MemoryMap::compare(), so a consistent
         * hash/equality pair falls out of the two.
         */
        int compare(const KState &other) const;
        //! True when \a other has the same identity as this state.
        bool operator==(const KState &other) const;
        //! Negation of operator==.
        bool operator!=(const KState &other) const;

    private:
        //! Instruction pointer; may be the end point.
        instr_ptr_t pc;
        //! Memory pointer.
        mem_ptr_t mem_ptr;
        //! Cell contents, with zero cells absent.
        MemoryMap memory;
        //! Reads left before the input ends, or no value for unbounded input.
        std::optional<std::size_t> remaining_stdin_chars;
        //! Hash of the fields above, computed once at construction.
        std::size_t state_hash;

        //! Recomputes \c state_hash from the current fields; called by the constructors.
        void update_hash();
    };

    //! `std::hash` replacement that asks a state for its cached hash.
    struct KStateHash
    {
        //! Cached hash of \a state; no field is read again.
        std::size_t operator()(const KState &state) const;
    };

    //! Equality of two states, matching \c KState::compare().
    struct KStateEqual
    {
        //! True when \a left and \a right are the same state.
        bool operator()(const KState &left, const KState &right) const;
    };

    //! Successors of one state, materialised on demand.
    /**
     * Only the transition itself is computed eagerly: the new instruction and
     * memory pointers, the number of characters left and the values the
     * transition can write. `state()` builds one successor when the search
     * asks for it, so a read does not construct all 256 states it may lead to.
     *
     * A state at the program's end point has exactly one successor: itself.
     * The program has halted, and stuttering is what makes a label behind the
     * last instruction visible and every run infinite (see
     * `spec/state-space.md`).
     */
    class KIterator
    {
    public:
        //! Applies the transition from \a state under \a prog.
        /**
         * \param state the state whose successors are enumerated
         * \param prog the program that gives the transition its meaning
         * \param folding which straight-line runs collapse into one transition
         */
        KIterator(const KState &state, const Program &prog,
                  Folding folding = Folding::straight_line);

        //! Rewinds to the first successor; false when there is none.
        bool first();
        //! Advances to the next successor; false when the last one was reached.
        bool next();
        //! True once every successor was visited.
        /**
         * Must stay `pos >= possible_values.size()`: the older
         * `pos >= size - 1` hid the last successor and every successor of a
         * deterministic instruction.
         */
        bool done() const;
        //! Fresh successor at the current position.
        /**
         * \pre the iterator is positioned on a successor, i.e. not \c done()
         */
        KState state() const;

    private:
        //! Instruction pointer of the successors.
        instr_ptr_t pc;
        //! Memory pointer of the successors.
        mem_ptr_t mem_ptr;
        //! Memory snapshot the successors derive from.
        MemoryMap memory;
        //! Input budget of the successors, after any read consumed one character.
        std::optional<std::size_t> remaining_stdin_chars;
        //! Values the transition can write to cell mem_ptr. Transitions that
        //! do not write hold a single placeholder and set writes_memory false.
        std::vector<cell_value_t> possible_values;
        //! Whether \c possible_values holds real cell values instead of a placeholder.
        bool writes_memory;
        //! Position inside \c possible_values.
        std::size_t pos;
    };

    //! The state graph of a program, with what a check observes in it.
    /**
     * A `Kripke` owns the program, the folding mode and the observation, and
     * nothing else: it is the entry point the analyses and `dot` use to get
     * the initial state, the successors of a state and the observed atom. The
     * search itself lives in `search.hpp`; see `spec/state-space.md` for the
     * state, transition and observation rules.
     */
    class Kripke
    {
    public:
        //! Builds the graph for \a program.
        /**
         * \param program the parsed program; stored by value (copies are cheap)
         * \param folding which straight-line runs collapse into one transition
         * \param observation what a check observes in each state; an empty
         *                    observation observes nothing
         */
        Kripke(Program program,
               Folding folding = Folding::straight_line,
               Observation observation = {});

        //! Initial state: pc 0, cell 0, empty memory and the IO model's input budget.
        KState initial_state() const;
        //! Successor iterator for \a state.
        KIterator successors(const KState &state) const;

        //! True when the observed atom holds in \a state.
        /**
         * This is the inner predicate of every property: `cell` for a visit,
         * `cell OP value` (plus the end point for `on_halt`) for a value, and
         * `the step leaving this state wraps the cell` for an overflow. With
         * an empty observation it is constant false, so a label check never
         * accidentally observes a cell.
         *
         * A wrap is the only variant that looks at a *transition*: it is
         * evaluated here, on the state the transition leaves, which is why the
         * witness ends on the wrapping instruction (see
         * `spec/cell-overflow.md`).
         */
        bool observes(const KState &state) const;

        //! The program this graph was built from.
        const Program &get_program() const;
        //! Which straight-line runs are folded.
        Folding get_folding() const;
        //! What a check observes in every state.
        const Observation &get_observation() const;

    private:
        //! The program this graph was built from.
        Program prog;
        //! Which straight-line runs are folded.
        Folding folding;
        //! What the analysis observes in every state.
        Observation observation;
    };
}
