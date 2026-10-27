#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>
#include <spot/kripke/kripke.hh>
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

    KState *KState::clone() const
    {
        // MemoryMap copies are O(1), so cloning a state is cheap.
        return new KState(this->pc, this->mem_ptr, this->memory, this->remaining_stdin_chars);
    }

    std::size_t KState::hash() const
    {
        return this->state_hash;
    }

    int KState::compare(const spot::state *other) const
    {
        const KState *o = static_cast<const KState *>(other);

        if (this->pc != o->pc)
            return this->pc < o->pc ? -1 : 1;
        if (this->mem_ptr != o->mem_ptr)
            return this->mem_ptr < o->mem_ptr ? -1 : 1;
        if (this->remaining_stdin_chars != o->remaining_stdin_chars)
        {
            // An unrestricted input (no value) sorts before any bound.
            if (!this->remaining_stdin_chars.has_value())
                return -1;
            if (!o->remaining_stdin_chars.has_value())
                return 1;
            return this->remaining_stdin_chars.value() < o->remaining_stdin_chars.value() ? -1 : 1;
        }
        return this->memory.compare(o->memory);
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

    KIterator::KIterator(const KState *state, const Program &prog, bdd cond, Folding folding)
        : spot::kripke_succ_iterator(cond),
          pc(state->get_instr_ptr()),
          mem_ptr(state->get_mem_ptr()),
          memory(state->get_memory_map()),
          remaining_stdin_chars(state->get_remaining_stdin_chars()),
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

    KState *KIterator::dst() const
    {
        MemoryMap successor_memory = this->memory;
        if (this->writes_memory)
            successor_memory = this->memory.set(this->mem_ptr, this->possible_values[this->pos]);
        return new KState(this->pc, this->mem_ptr, std::move(successor_memory), this->remaining_stdin_chars);
    }

    Kripke::Kripke(Program program, const spot::bdd_dict_ptr &d, Folding folding,
                   Observation observation)
        : spot::kripke(d),
          prog(std::move(program)),
          folding(folding),
          observation(observation),
          cell_bdd(bdd_false()),
          value_bdd(bdd_false()),
          wrap_bdd(bdd_false()),
          halt_bdd(bdd_false())
    {
        // The cell, value, wrap and halt atoms are only part of the state
        // condition when a check asks for them. Registering them here maps the
        // names in the analysis formula to the same BDD variables.
        if (this->observation.visited_cell)
        {
            this->cell_bdd = bdd_ithvar(register_ap(CELL_VISIT_PROPOSITION));
            // A visit check may exclude the end point, so it always needs the
            // halt atom; a value check needs it only for a postcondition.
            this->halt_bdd = bdd_ithvar(register_ap(HALT_PROPOSITION));
        }
        if (this->observation.cell_test)
        {
            this->value_bdd = bdd_ithvar(register_ap(CELL_VALUE_PROPOSITION));
            if (this->observation.cell_test->on_halt)
                this->halt_bdd = bdd_ithvar(register_ap(HALT_PROPOSITION));
        }
        if (this->observation.wrap_test)
            this->wrap_bdd = bdd_ithvar(register_ap(CELL_WRAP_PROPOSITION));

        // Register one atomic proposition per label **name**. A name may occur
        // at several pointers, and it is one proposition that is true at each
        // of them; building the literal per label-map entry would turn such a
        // name into `name & !name` and make every pointer that carries it
        // unlabelled (an unlabelled state is a dead end for the product, which
        // silently turns every answer into a vacuous one).
        struct LabelGroup
        {
            bdd condition;                  //!< proposition of one label name
            std::set<instr_ptr_t> pointers; //!< every pointer that carries the name
        };
        std::vector<LabelGroup> labels;
        std::map<std::string, std::size_t> group_of_name;
        for (const auto &entry : this->prog.get_label_map())
        {
            auto found = group_of_name.find(entry.second);
            if (found == group_of_name.end())
            {
                found = group_of_name.emplace(entry.second, labels.size()).first;
                labels.push_back(LabelGroup{bdd_ithvar(register_ap(entry.second)), {}});
            }
            labels[found->second].pointers.insert(entry.first);
        }

        const instr_ptr_t pointers = this->prog.instr_count() + 1;
        if (labels.empty())
        {
            // Without labels no state is distinguished. An observation still
            // needs a condition, so its literal can be conjoined; otherwise
            // keep the states inert instead of accepting everything.
            if (this->observation.visited_cell || this->observation.cell_test ||
                this->observation.wrap_test)
                this->state_conditions.assign(pointers, bdd_true());
            else
                this->state_conditions.assign(pointers, bdd_false());
            return;
        }

        // A state is labelled true exactly where its pointer carries a label
        // name and false everywhere else, so the condition is a function of the
        // pointer alone and can be built once here.
        bdd no_label = bdd_true();
        for (const auto &label : labels)
            no_label &= !label.condition;

        this->state_conditions.assign(pointers, no_label);

        // Every pointer that carries at least one label gets its own condition:
        // a name is true exactly at the pointers that carry it. The label map is
        // ordered by pointer, so the distinct pointers are the first of each run
        // of equal keys.
        instr_ptr_t labelled_pc = 0;
        bool have_pointer = false;
        for (const auto &entry : this->prog.get_label_map())
        {
            if (have_pointer && entry.first == labelled_pc)
                continue;
            labelled_pc = entry.first;
            have_pointer = true;

            bdd condition = bdd_true();
            for (const auto &label : labels)
                condition &= label.pointers.count(labelled_pc) != 0 ? label.condition
                                                                   : !label.condition;
            this->state_conditions[labelled_pc] = condition;
        }
    }

    KState *Kripke::get_init_state() const
    {
        return new KState(this->prog.io_model.get_chars_until_eof());
    }

    KIterator *Kripke::succ_iter(const spot::state *s) const
    {
        const KState *state = static_cast<const KState *>(s);
        return new KIterator(state, this->prog, state_condition(state), this->folding);
    }

    bdd Kripke::state_condition(const spot::state *s) const
    {
        const KState *state = static_cast<const KState *>(s);
        const instr_ptr_t pc = state->get_instr_ptr();
        if (pc >= this->state_conditions.size())
            return bdd_false();

        bdd condition = this->state_conditions[pc];
        if (this->observation.visited_cell)
        {
            // The pointer position is part of the condition, so pointer runs
            // must stay unfolded (see Folding::values).
            condition &= state->get_mem_ptr() == this->observation.visited_cell.value()
                             ? this->cell_bdd
                             : !this->cell_bdd;
        }
        if (this->observation.cell_test)
        {
            // The cell's content is part of the condition, so `+`/`-` runs must
            // stay unfolded (see Folding::pointers). This is a function of the
            // whole state, which is what Spot labels a state's transitions
            // with, so successors that differ in their memory contents are fine.
            const CellTest &test = this->observation.cell_test.value();
            condition &= cell_test_holds(test, state->get_memory_map().get(test.cell))
                             ? this->value_bdd
                             : !this->value_bdd;
        }
        if (this->observation.wrap_test)
        {
            // A wrap happens on the step *leaving* this state, which Spot labels
            // with this condition, so the literal belongs to the state before
            // the wrapping instruction. The step is one `+`/`-`, so value runs
            // must stay unfolded (see Folding::pointers).
            condition &= wrap_test_holds(this->prog, *state, this->observation.wrap_test.value())
                             ? this->wrap_bdd
                             : !this->wrap_bdd;
        }
        if (this->halt_bdd != bdd_false())
        {
            // No check observes the halt point when nothing registered the atom
            // (a plain value check). Otherwise the end point stutters; excluding
            // it is what makes an infinitely-often check mean "before halting",
            // and a postcondition is only true there.
            condition &= this->prog.is_end(pc) ? this->halt_bdd : !this->halt_bdd;
        }
        return condition;
    }

    std::string Kripke::format_state(const spot::state *s) const
    {
        const KState *state = static_cast<const KState *>(s);
        std::ostringstream out;
        out << "pc = " << state->get_instr_ptr()
            << "; mp = " << state->get_mem_ptr()
            << "\nhash = " << state->hash();
        return out.str();
    }
}
