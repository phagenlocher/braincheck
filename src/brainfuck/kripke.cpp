#include <map>
#include <optional>
#include <utility>
#include <vector>
#include "kripke.hpp"

namespace brainfuck
{
    bool cell_test_holds(const CellTest &test, cell_value_t content)
    {
        const std::uint64_t value = static_cast<std::uint64_t>(content);
        switch (test.op)
        {
        case Comparison::eq:
            return value == test.value;
        case Comparison::gt:
            return value > test.value;
        case Comparison::ge:
            return value >= test.value;
        case Comparison::lt:
            return value < test.value;
        case Comparison::le:
            return value <= test.value;
        }
        return false;
    }

    bool wrap_test_holds(const Program &prog, const KState &state, const WrapTest &test)
    {
        if (state.get_mem_ptr() != test.cell)
            // Only the cell under the pointer changes, so a step that leaves
            // the pointer elsewhere cannot wrap the observed cell.
            return false;
        const std::optional<Instruction> instr = prog.instr_for_pc(state.get_instr_ptr());
        if (!instr.has_value())
            // The end point stutters; it contains no wrapping instruction.
            return false;

        const cell_value_t content = state.get_memory_map().get(test.cell);
        const bool overflow =
            instr.value() == Instruction::inc && content == prog.memory_model.get_max_value();
        const bool underflow = instr.value() == Instruction::dec && content == 0;
        switch (test.direction)
        {
        case WrapDirection::overflow:
            return overflow;
        case WrapDirection::underflow:
            return underflow;
        case WrapDirection::both:
            return overflow || underflow;
        }
        return false;
    }

    KState::KState()
        : KState(std::nullopt)
    {
    }

    KState::KState(std::optional<std::size_t> chars)
        : pc(0), mem_ptr(0), memory(), remaining_stdin_chars(chars)
    {
        update_hash();
    }

    KState::KState(instr_ptr_t pc, mem_ptr_t mem_ptr, const std::map<mem_ptr_t, cell_value_t> &values)
        : KState(pc, mem_ptr, MemoryMap(values), std::nullopt)
    {
    }

    KState::KState(
        instr_ptr_t pc,
        mem_ptr_t mem_ptr,
        const std::map<mem_ptr_t, cell_value_t> &values,
        std::optional<std::size_t> chars)
        : KState(pc, mem_ptr, MemoryMap(values), chars)
    {
    }

    KState::KState(instr_ptr_t pc, mem_ptr_t mem_ptr, MemoryMap memory_map, std::optional<std::size_t> chars)
        : pc(pc), mem_ptr(mem_ptr), memory(std::move(memory_map)), remaining_stdin_chars(chars)
    {
        update_hash();
    }

    instr_ptr_t KState::get_instr_ptr() const
    {
        return this->pc;
    }

    mem_ptr_t KState::get_mem_ptr() const
    {
        return this->mem_ptr;
    }

    std::optional<std::size_t> KState::get_remaining_stdin_chars() const
    {
        return this->remaining_stdin_chars;
    }

    const MemoryMap &KState::get_memory_map() const
    {
        return this->memory;
    }

    std::map<mem_ptr_t, cell_value_t> KState::get_memory() const
    {
        return this->memory.to_std_map();
    }

    std::size_t KState::hash() const
    {
        return this->state_hash;
    }

    int KState::compare(const KState &other) const
    {
        if (this->pc != other.pc)
            return this->pc < other.pc ? -1 : 1;
        if (this->mem_ptr != other.mem_ptr)
            return this->mem_ptr < other.mem_ptr ? -1 : 1;
        if (this->remaining_stdin_chars != other.remaining_stdin_chars)
        {
            // An unrestricted input (no value) sorts before any bound.
            if (!this->remaining_stdin_chars.has_value())
                return -1;
            if (!other.remaining_stdin_chars.has_value())
                return 1;
            return this->remaining_stdin_chars.value() < other.remaining_stdin_chars.value() ? -1 : 1;
        }
        return this->memory.compare(other.memory);
    }

    bool KState::operator==(const KState &other) const
    {
        return this->compare(other) == 0;
    }

    bool KState::operator!=(const KState &other) const
    {
        return !(*this == other);
    }

    void KState::update_hash()
    {
        std::size_t hash = mix_hash(this->pc);
        hash = mix_hash(hash ^ this->mem_ptr);
        hash = mix_hash(hash ^ this->memory.hash());
        if (this->remaining_stdin_chars.has_value())
            hash = mix_hash(hash ^ (this->remaining_stdin_chars.value() + 1));
        this->state_hash = hash;
    }

    std::size_t KStateHash::operator()(const KState &state) const
    {
        return state.hash();
    }

    bool KStateEqual::operator()(const KState &left, const KState &right) const
    {
        return left == right;
    }

    KIterator::KIterator(const KState &state, const Program &prog, Folding folding)
        : pc(state.get_instr_ptr()),
          mem_ptr(state.get_mem_ptr()),
          memory(state.get_memory_map()),
          remaining_stdin_chars(state.get_remaining_stdin_chars()),
          writes_memory(false),
          pos(0)
    {
        if (prog.is_end(this->pc))
        {
            // The program has halted. Keep the state alive with a self-loop so
            // that a label behind the last instruction is attached to a state
            // with a successor (and therefore visible in `dot`), and so that a
            // run which halts without visiting a label is still an infinite
            // run and can be reported as a counterexample.
            this->possible_values.push_back(0);
            return;
        }

        const Instruction instruction = prog.instr_for_pc(this->pc).value();
        const MemoryModel &memory_model = prog.memory_model;
        const cell_value_t current_cell = this->memory.get(this->mem_ptr);
        // A pointer run is only folded when the property does not observe the
        // pointer. In Folding::values a state may therefore be an interior
        // instruction of a pointer block, and the block table's whole-run
        // entry must not be applied: one move is one transition.
        Block block{BlockKind::single, this->pc + 1, 0, 0, 0};
        if (folding != Folding::none)
        {
            const Block candidate = prog.block_at(this->pc);
            // Symmetrically, a value run is only folded when the property does
            // not observe a cell's content: a folded `+++++` would hide every
            // intermediate value.
            const bool fold_candidate =
                folding == Folding::straight_line ||
                (folding == Folding::values && candidate.kind != BlockKind::pointers) ||
                (folding == Folding::pointers && candidate.kind == BlockKind::pointers);
            if (fold_candidate)
                block = candidate;
        }

        if (block.kind == BlockKind::values)
        {
            // A whole run of + and - is one transition; the intermediate values
            // are not states because no label can sit inside the run and the
            // pointer does not move, so no cell position is hidden either.
            this->writes_memory = true;
            this->possible_values.push_back(
                memory_model.shift_value(current_cell, block.delta));
            this->pc = block.end;
        }
        else if (block.kind == BlockKind::pointers)
        {
            this->mem_ptr = memory_model.move_pointer(
                this->mem_ptr, block.delta, block.low, block.high);
            this->pc = block.end;
        }
        else
        {
            switch (instruction)
            {
            case Instruction::left:
                this->mem_ptr = memory_model.pointer_left(this->mem_ptr);
                this->pc++;
                break;

            case Instruction::right:
                this->mem_ptr = memory_model.pointer_right(this->mem_ptr);
                this->pc++;
                break;

            case Instruction::inc:
                this->writes_memory = true;
                this->possible_values.push_back(memory_model.next_value(current_cell));
                this->pc++;
                break;

            case Instruction::dec:
                this->writes_memory = true;
                this->possible_values.push_back(memory_model.prev_value(current_cell));
                this->pc++;
                break;

            case Instruction::get:
            {
                this->writes_memory = true;
                const std::vector<io_char_t> chars =
                    prog.io_model.get_possible_chars(this->remaining_stdin_chars);
                this->possible_values.reserve(chars.size());
                for (const io_char_t c : chars)
                    this->possible_values.push_back(memory_model.normalize_value(c));
                if (this->possible_values.empty())
                    // End of input without a change of the cell.
                    this->possible_values.push_back(current_cell);

                // Only a read consumes input, and the budget is part of the state.
                if (this->remaining_stdin_chars.has_value() && this->remaining_stdin_chars.value() > 0)
                    this->remaining_stdin_chars = this->remaining_stdin_chars.value() - 1;
                this->pc++;
                break;
            }

            case Instruction::put:
                this->pc++;
                break;

            case Instruction::fwd:
                this->pc = current_cell == 0 ? prog.jump_target(this->pc) : this->pc + 1;
                break;

            case Instruction::bwd:
                this->pc = current_cell != 0 ? prog.jump_target(this->pc) : this->pc + 1;
                break;
            }
        }

        // A transition that writes no cell still has exactly one successor.
        // The placeholder value is never read because writes_memory is false.
        if (this->possible_values.empty())
            this->possible_values.push_back(0);
    }

    bool KIterator::first()
    {
        this->pos = 0;
        return !this->possible_values.empty();
    }

    bool KIterator::next()
    {
        this->pos++;
        return this->pos < this->possible_values.size();
    }

    bool KIterator::done() const
    {
        return this->pos >= this->possible_values.size();
    }

    KState KIterator::state() const
    {
        MemoryMap successor_memory = this->memory;
        if (this->writes_memory)
            successor_memory = this->memory.set(this->mem_ptr, this->possible_values[this->pos]);
        return KState(this->pc, this->mem_ptr, std::move(successor_memory), this->remaining_stdin_chars);
    }

    Kripke::Kripke(Program program, Folding folding, Observation observation)
        : prog(std::move(program)), folding(folding), observation(observation)
    {
    }

    KState Kripke::initial_state() const
    {
        return KState(this->prog.io_model.get_chars_until_eof());
    }

    KIterator Kripke::successors(const KState &state) const
    {
        return KIterator(state, this->prog, this->folding);
    }

    bool Kripke::observes(const KState &state) const
    {
        if (this->observation.visited_cell)
            // The pointer position is part of the atom, so pointer runs must
            // stay unfolded (see Folding::values).
            return state.get_mem_ptr() == this->observation.visited_cell.value();

        if (this->observation.cell_test)
        {
            // A cell's content is part of the atom, so `+`/`-` runs must stay
            // unfolded (see Folding::pointers). A postcondition observes only
            // the end point, because the halt literal is part of the atom.
            const CellTest &test = this->observation.cell_test.value();
            if (test.on_halt && !this->prog.is_end(state.get_instr_ptr()))
                return false;
            return cell_test_holds(test, state.get_memory_map().get(test.cell));
        }

        if (this->observation.wrap_test)
            // A wrap happens on the step *leaving* this state, so the atom
            // belongs to the state before the wrapping instruction. The step is
            // one `+`/`-`, so value runs must stay unfolded (see
            // Folding::pointers).
            return wrap_test_holds(this->prog, state, this->observation.wrap_test.value());

        // Nothing is observed: a label check or `dot`, where the atom is the
        // label itself and has no per-state cell part.
        return false;
    }

    const Program &Kripke::get_program() const
    {
        return this->prog;
    }

    Folding Kripke::get_folding() const
    {
        return this->folding;
    }

    const Observation &Kripke::get_observation() const
    {
        return this->observation;
    }
}
