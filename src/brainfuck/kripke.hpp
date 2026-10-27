#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <spot/kripke/kripke.hh>
#include "memory_map.hpp"
#include "model.hpp"
#include "program.hpp"

namespace brainfuck
{
    class KState;

    //! Which straight-line runs the Kripke layer folds away.
    /**
     * A folded run removes every instruction pointer inside it, so a property
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
     * condition is therefore attached to the state *before* the step, whose
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
     * A check observes at most one of the three. An empty observation yields
     * the same conditions as before, which is what `dot` and `check reach` use.
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

    //! Atomic proposition that marks the visited cell in a cell-visit analysis.
    /**
     * Contains an underscore, so it can never collide with a label name: the
     * parser takes everything between two underscores as a label, and a label
     * name therefore never contains one.
     */
    const char *const CELL_VISIT_PROPOSITION = "braincheck_cell_visited";
    //! Atomic proposition that is true exactly at the program's end point.
    /**
     * Also contains an underscore, so it cannot collide with a label name. A
     * cell-visit check conjoins it with the cell proposition: the end point
     * stutters, so a property about visits *before halting* has to exclude it.
     */
    const char *const HALT_PROPOSITION = "braincheck_halt";
    //! Atomic proposition that marks a state whose observed cell satisfies the test.
    /**
     * Also contains an underscore, so it cannot collide with a label name. The
     * proposition is only registered when a value check observes a cell.
     */
    const char *const CELL_VALUE_PROPOSITION = "braincheck_cell_value";
    //! Atomic proposition that marks a state whose next step wraps the observed cell.
    /**
     * Also contains an underscore, so it cannot collide with a label name. The
     * proposition is only registered when an overflow check observes a cell.
     */
    const char *const CELL_WRAP_PROPOSITION = "braincheck_cell_wrap";

    //! One state of the Kripke structure induced by a program.
    /**
     * A state is the instruction pointer, the memory pointer, the memory
     * snapshot and the number of characters still available on stdin.
     * The memory is a MemoryMap, so cloning a state is O(1).
     *
     * hash() and compare() must agree: both look at every field above,
     * memory contents included. Otherwise Spot's hash tables store states
     * that compare equal, exploration grows without bound and the analysis
     * merges states that are not actually the same.
     */
    class KState : public spot::state
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
         * The hash is computed once here, because Spot asks for it repeatedly
         * and the fields never change afterwards.
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

        //! Deep copy owned by the caller; cheap because the memory is shared.
        KState *clone() const override;
        //! Cached hash over pc, memory pointer, memory contents and input budget.
        std::size_t hash() const override;
        //! Total order over exactly the fields that \c hash() covers.
        /**
         * Spot requires a strict weak ordering consistent with equality. An
         * unbounded input sorts before every bounded one, and the memory is
         * compared last through \c MemoryMap::compare().
         */
        int compare(const spot::state *other) const override;

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

    //! Successors of one state, materialised on demand.
    /**
     * Only the transition itself is computed eagerly: the new instruction and
     * memory pointers, the number of characters left and the values the
     * transition can write. dst() builds one successor when Spot asks for it,
     * so a read does not construct all 256 states it may lead to.
     *
     * A state at the program's end point has exactly one successor: itself. The
     * program has halted, and staying halted is what the model checker needs to
     * see a label behind the last instruction (see `spec/state-space.md`).
     */
    class KIterator : public spot::kripke_succ_iterator
    {
    public:
        //! Applies the transition from \a state under \a prog.
        /**
         * \param state the state whose successors are enumerated
         * \param prog the program that gives the transition its meaning
         * \param cond the state condition Spot attaches to the successors
         * \param folding which straight-line runs collapse into one transition
         */
        KIterator(const KState *state, const Program &prog, bdd cond,
                  Folding folding = Folding::straight_line);

        //! Rewinds to the first successor; false when there is none.
        bool first() override;
        //! Advances to the next successor; false when the last one was reached.
        bool next() override;
        //! True once every successor was visited.
        /**
         * Must stay `pos >= possible_values.size()`: the older
         * `pos >= size - 1` hid the last successor and every successor of a
         * deterministic instruction.
         */
        bool done() const override;
        //! Fresh successor at the current position, owned by the caller.
        KState *dst() const override;

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

    //! The program as a Kripke structure for Spot.
    /**
     * Registers every label as an atomic proposition and attaches one state
     * condition per instruction pointer, so a state is labelled true exactly
     * where its pointer carries a label. A cell-visit analysis additionally
     * tracks one cell's pointer position, a value analysis one cell's content
     * and an overflow analysis the wrap the next step would cause; the
     * conditions then also depend on the memory pointer, the memory contents,
     * the instruction about to run and the end point (see
     * `spec/state-space.md`, `spec/cell-visits.md`, `spec/cell-values.md` and
     * `spec/cell-overflow.md`).
     */
    class Kripke : public spot::kripke
    {
    public:
        //! Builds the structure for \a program, using \a d for propositions.
        /**
         * \param program the parsed program; stored by value (copies are cheap)
         * \param d Spot's BDD dictionary that owns the propositions
         * \param folding which straight-line runs collapse into one transition
         * \param observation what a check observes in each state; an empty
         *                    observation adds no propositions
         */
        Kripke(Program program, const spot::bdd_dict_ptr &d,
               Folding folding = Folding::straight_line,
               Observation observation = {});

        //! Initial state: pc 0, cell 0, empty memory and the IO model's input budget.
        KState *get_init_state() const override;
        //! Successor iterator for \a s, labelled with the state's condition.
        KIterator *succ_iter(const spot::state *s) const override;
        //! Condition of \a s: its labels, plus the observed cell, value and wrap atoms.
        /**
         * The condition is a function of the whole state - instruction
         * pointer, memory pointer, memory contents and the end point - and
         * never of which successor is taken. Spot labels every transition that
         * leaves a state with that state's condition, so a condition may look
         * at cell contents even though a read has successors that differ in
         * them; a wrap condition is true in the state *before* the wrapping
         * step for the same reason. Pointers outside the program (there are
         * none in practice) yield \c bdd_false().
         */
        bdd state_condition(const spot::state *s) const override;
        //! Multi-line textual description of \a s for Spot's diagnostics.
        std::string format_state(const spot::state *s) const override;

    private:
        //! The program this structure was built from.
        Program prog;
        //! Which straight-line runs are folded.
        Folding folding;
        //! What the analysis observes in every state.
        Observation observation;
        //! BDD of \c CELL_VISIT_PROPOSITION; only meaningful for a visit.
        bdd cell_bdd;
        //! BDD of \c CELL_VALUE_PROPOSITION; only meaningful for a value check.
        bdd value_bdd;
        //! BDD of \c CELL_WRAP_PROPOSITION; only meaningful for an overflow check.
        bdd wrap_bdd;
        //! BDD of \c HALT_PROPOSITION; registered when an observation needs it.
        bdd halt_bdd;
        //! Label condition by instruction pointer, terminal pointer included.
        std::vector<bdd> state_conditions;
    };
}
