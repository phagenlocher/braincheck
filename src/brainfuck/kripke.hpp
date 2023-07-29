#pragma once

#include <map>
#include <vector>
#include <optional>
#include <spot/kripke/kripke.hh>
#include "program.hpp"
#include "model.hpp"

namespace brainfuck
{
    class KState : public spot::state
    {
    private:
        instr_ptr_t pc;
        mem_ptr_t mem_ptr;
        std::map<mem_ptr_t, uint8_t> memory;
        std::optional<unsigned int> m_remaining_stdin_chars;

    public:
        KState();
        KState(instr_ptr_t pc, mem_ptr_t mem_ptr, std::map<mem_ptr_t, uint8_t> memory);
        KState(instr_ptr_t pc, mem_ptr_t mem_ptr, std::map<mem_ptr_t, uint8_t> memory, std::optional<unsigned int> m_remaining_stdin_chars);
        KState(std::optional<unsigned int> m_remaining_stdin_chars);
        instr_ptr_t get_instr_ptr() const;
        mem_ptr_t get_mem_ptr() const;
        std::optional<unsigned int> get_remaining_stdin_chars() const;
        std::map<mem_ptr_t, uint8_t> get_memory() const;
        KState *clone() const override;
        size_t hash() const override;
        int compare(const spot::state *other) const override;
    };

    class KIterator : public spot::kripke_succ_iterator
    {
    private:
        std::vector<KState> states;
        unsigned long pos;
        Program prog;

    public:
        KIterator(const KState *state, Program prog, bdd cond);
        bool first() override;
        bool next() override;
        bool done() const override;
        KState *dst() const override;
    };

    class Kripke : public spot::kripke
    {
    private:
        std::vector<std::pair<instr_ptr_t, bdd>> labels;
        Program prog;

    public:
        Kripke(Program prog, const spot::bdd_dict_ptr &d);
        KState *get_init_state() const override;
        KIterator *succ_iter(const spot::state *s) const override;
        bdd state_condition(const spot::state *s) const override;
        std::string format_state(const spot::state *s) const override;
    };
}