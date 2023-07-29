#include <map>
#include <vector>
#include <iostream>
#include <spot/kripke/kripke.hh>
#include "kripke.hpp"

namespace brainfuck
{
    template <class T>
    void hash_combine(std::size_t &seed, const T &v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) * 0xABCDEF + 0x9e3779b9 + (seed << 16);
    }

    KState::KState()
    {
        this->pc = 0;
        this->mem_ptr = 0;
        std::map<mem_ptr_t, uint8_t> memory{};
        this->memory = memory;
        this->m_remaining_stdin_chars = std::nullopt;
    }

    KState::KState(instr_ptr_t pc, mem_ptr_t mem_ptr, std::map<mem_ptr_t, uint8_t> memory)
    {
        this->pc = pc;
        this->mem_ptr = mem_ptr;
        this->memory = memory;
        this->m_remaining_stdin_chars = std::nullopt;
    }

    KState::KState(instr_ptr_t pc,
                   mem_ptr_t mem_ptr,
                   std::map<mem_ptr_t, uint8_t> memory,
                   std::optional<unsigned int> m_remaining_stdin_chars)
    {
        this->pc = pc;
        this->mem_ptr = mem_ptr;
        this->memory = memory;
        this->m_remaining_stdin_chars = m_remaining_stdin_chars;
    }

    KState::KState(std::optional<unsigned int> m_remaining_stdin_chars)
    {
        this->pc = 0;
        this->mem_ptr = 0;
        std::map<mem_ptr_t, uint8_t> memory{};
        this->memory = memory;
        this->m_remaining_stdin_chars = m_remaining_stdin_chars;
    }

    instr_ptr_t KState::get_instr_ptr() const
    {
        return this->pc;
    }

    mem_ptr_t KState::get_mem_ptr() const
    {
        return this->mem_ptr;
    }

    std::map<mem_ptr_t, uint8_t> KState::get_memory() const
    {
        return this->memory;
    }

    std::optional<unsigned int> KState::get_remaining_stdin_chars() const
    {
        return this->m_remaining_stdin_chars;
    }

    KState *KState::clone() const
    {
        return new KState(this->pc, this->mem_ptr, this->memory, this->m_remaining_stdin_chars);
    }

    size_t KState::hash() const
    {
        size_t hash = 0xC0FFEE;
        hash_combine(hash, this->pc);
        hash_combine(hash, this->mem_ptr);
        for (const auto &kv : this->memory)
        {
            if (kv.second == 0)
                continue;
            hash_combine(hash, kv.first);
            hash_combine(hash, kv.second);
        }
        if (this->m_remaining_stdin_chars.has_value())
            hash_combine(hash, this->m_remaining_stdin_chars.value());
        return hash;
    }

    int KState::compare(const spot::state *other) const
    {
        auto o = static_cast<const KState *>(other);

        if (this->pc != o->pc)
        {
            return this->pc - o->pc;
        }
        else if (this->mem_ptr != o->mem_ptr)
        {
            return this->mem_ptr - o->mem_ptr;
        }
        else if (this->m_remaining_stdin_chars.has_value())
        {
            if (o->m_remaining_stdin_chars.has_value())
            {
                unsigned int this_chars = this->m_remaining_stdin_chars.value();
                unsigned int other_chars = o->m_remaining_stdin_chars.value();
                if (this_chars != other_chars)
                {
                    return this_chars - other_chars;
                }
            }
            else
            {
                return 1;
            }
        }
        else if (o->m_remaining_stdin_chars.has_value())
        {
            // We already know that this->m_remaining_stdin_chars has no value
            return -1;
        }
        return 0;
    }

    KIterator::KIterator(const KState *state, Program prog, bdd cond)
        : kripke_succ_iterator(cond)
    {
        this->pos = 0;
        this->prog = prog;

        // std::cerr << "KIterator for " << state->hash() << ": ";

        instr_ptr_t pc = state->get_instr_ptr();
        mem_ptr_t mem_ptr = state->get_mem_ptr();
        auto memory = state->get_memory();
        std::optional<unsigned int> m_remaining_stdin_chars = state->get_remaining_stdin_chars();
        std::optional<unsigned int> m_new_remaining_chars;
        if (m_remaining_stdin_chars.has_value())
        {
            unsigned int cur = m_remaining_stdin_chars.value();
            m_new_remaining_chars = std::make_optional(cur > 0 ? cur - 1 : 0);
        }
        else
        {
            m_new_remaining_chars = std::nullopt;
        }

        auto current_cell = memory.count(mem_ptr) > 0 ? memory[mem_ptr] : 0;
        auto m_instr = prog.instr_for_pc(pc);
        std::vector<uint8_t> possible_vals;
        if (m_instr.has_value())
        {
            switch (m_instr.value())
            {
            case Instruction::left:
                mem_ptr = mem_ptr == 0 ? 29999 : mem_ptr - 1;
                pc++;
                break;

            case Instruction::right:
                mem_ptr = mem_ptr == 29999 ? 0 : mem_ptr + 1;
                pc++;
                break;

            case Instruction::inc:
                possible_vals.push_back((current_cell + 1) % 256);
                pc++;
                break;

            case Instruction::dec:
                possible_vals.push_back((current_cell - 1) % 256);
                pc++;
                break;

            case Instruction::get:
                possible_vals = prog.io_model.get_possible_chars(m_remaining_stdin_chars);
                if (possible_vals.size() == 0)
                {
                    possible_vals.push_back(current_cell);
                }
                pc++;
                break;

            case Instruction::put:
                pc++;
                break;

            case Instruction::fwd:
                if (current_cell == 0)
                    pc = prog.get_jmp_map().at(pc);
                else
                    pc++;
                break;

            case Instruction::bwd:
                if (current_cell != 0)
                    pc = prog.get_jmp_map().at(pc);
                else
                    pc++;
                break;

            default:
                abort();
            }

            std::vector<KState> states;
            if (possible_vals.empty())
            {
                std::map<mem_ptr_t, uint8_t> new_memory(memory);
                states.push_back(KState(pc, mem_ptr, new_memory, m_new_remaining_chars));
            }
            else
            {
                for (const auto &val : possible_vals)
                {
                    std::map<mem_ptr_t, uint8_t> new_memory(memory);
                    if (val == 0)
                        new_memory.erase(mem_ptr);
                    else
                        new_memory[mem_ptr] = val;
                    states.push_back(KState(pc, mem_ptr, new_memory, m_new_remaining_chars));
                }
            }
            this->states = states;
        }
        else
        {
            this->states = {};
        }

        // std::cerr << this->states.size() << " states" << std::endl;
    }

    bool KIterator::first()
    {
        this->pos = 0;
        return this->states.size() > 0;
    }

    bool KIterator::next()
    {
        this->pos++;
        return this->pos < this->states.size();
    }

    bool KIterator::done() const
    {
        return this->pos >= this->states.size() - 1;
    }

    KState *KIterator::dst() const
    {
        return this->states[this->pos].clone();
    }

    Kripke::Kripke(Program prog, const spot::bdd_dict_ptr &d)
        : spot::kripke(d)
    {
        std::vector<std::pair<instr_ptr_t, bdd>> labels;
        for (const auto &kv : prog.get_label_map())
            labels.push_back(std::make_pair(kv.first, bdd_ithvar(register_ap(kv.second))));
        this->labels = labels;
        this->prog = prog;
    }

    KState *Kripke::get_init_state() const
    {
        auto m_remaining_stdin_chars = this->prog.io_model.get_chars_until_eof();
        return new KState(m_remaining_stdin_chars);
    }

    KIterator *Kripke::succ_iter(const spot::state *s) const
    {
        auto ss = static_cast<const KState *>(s);
        return new KIterator(ss, this->prog, state_condition(ss));
    }

    bdd Kripke::state_condition(const spot::state *s) const
    {
        bdd cond;
        auto ss = static_cast<const KState *>(s);
        instr_ptr_t pc = ss->get_instr_ptr();
        std::pair<instr_ptr_t, bdd> head;
        if (this->labels.size() == 0)
        {
            return bdd_false();
        }
        else
        {
            head = this->labels.front();
            cond = (head.first == pc ? head.second : !head.second);
        }
        for (const auto &l : this->labels)
        {
            if (&l == &head)
                continue;
            cond &= (l.first == pc ? l.second : !l.second);
        }
        return cond;
    }

    std::string Kripke::format_state(const spot::state *s) const
    {
        auto ss = static_cast<const KState *>(s);
        std::ostringstream out;
        out << "pc = "
            << ss->get_instr_ptr()
            << "; mp = "
            << ss->get_mem_ptr()
            << "\nhash = " << ss->hash();
        return out.str();
    }
}