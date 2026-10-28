#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stack>
#include <utility>
#include "program.hpp"

namespace brainfuck
{
    char instr_char(Instruction instr)
    {
        switch (instr)
        {
        case Instruction::left:
            return '<';
        case Instruction::right:
            return '>';
        case Instruction::inc:
            return '+';
        case Instruction::dec:
            return '-';
        case Instruction::get:
            return ',';
        case Instruction::put:
            return '.';
        case Instruction::fwd:
            return '[';
        case Instruction::bwd:
            return ']';
        }
        std::abort();
    }

    namespace
    {
        //! True for `+` or `-`, the instructions a value block folds.
        bool is_value_instruction(Instruction instr)
        {
            return instr == Instruction::inc || instr == Instruction::dec;
        }

        //! True for `>` or `<`, the instructions a pointer block folds.
        bool is_pointer_instruction(Instruction instr)
        {
            return instr == Instruction::left || instr == Instruction::right;
        }

        //! Folds maximal runs of + - and > < into blocks.
        /**
         * A run stops at the first instruction of another class and before any
         * labelled pc, because a label has to stay observable as its own state.
         * The bounds of a pointer block are composed here once, so a clamping
         * tape stays exact without rescanning the run per state.
         *
         * The result has one entry per instruction pointer, so `block_at(pc)`
         * is a plain lookup even though only block starts are ever queried.
         */
        std::shared_ptr<std::vector<Block>> build_blocks(
            const std::vector<Instruction> &ops,
            const label_map_t &labels,
            const MemoryModel &memory_model)
        {
            auto blocks = std::make_shared<std::vector<Block>>();
            blocks->reserve(ops.size());
            const memory_size_t memory_size = memory_model.get_memory_size();
            const int64_t cell_modulus =
                static_cast<int64_t>(memory_model.get_max_value()) + 1;

            instr_ptr_t pc = 0;
            while (pc < ops.size())
            {
                const Instruction instr = ops[pc];

                if (is_value_instruction(instr))
                {
                    int64_t delta = 0;
                    instr_ptr_t end = pc;
                    while (end < ops.size() &&
                           is_value_instruction(ops[end]) &&
                           (end == pc || labels.count(end) == 0))
                    {
                        delta += ops[end] == Instruction::inc ? 1 : -1;
                        end++;
                    }
                    // A run longer than the cell width still only matters modulo
                    // the width, and reducing here cannot overflow the counter.
                    const Block block{BlockKind::values, end, delta % cell_modulus, 0, memory_size - 1};
                    for (instr_ptr_t i = pc; i < end; i++)
                        blocks->push_back(block);
                    pc = end;
                }
                else if (is_pointer_instruction(instr))
                {
                    int64_t net = 0;
                    mem_ptr_t low = 0;
                    mem_ptr_t high = memory_size - 1;
                    instr_ptr_t end = pc;
                    while (end < ops.size() &&
                           is_pointer_instruction(ops[end]) &&
                           (end == pc || labels.count(end) == 0))
                    {
                        if (ops[end] == Instruction::right)
                        {
                            net++;
                            if (low < memory_size - 1)
                                low++;
                            if (high < memory_size - 1)
                                high++;
                        }
                        else
                        {
                            net--;
                            if (low > 0)
                                low--;
                            if (high > 0)
                                high--;
                        }
                        end++;
                    }
                    const Block block{BlockKind::pointers, end, net, low, high};
                    for (instr_ptr_t i = pc; i < end; i++)
                        blocks->push_back(block);
                    pc = end;
                }
                else
                {
                    blocks->push_back({BlockKind::single, pc + 1, 0, 0, memory_size - 1});
                    pc++;
                }
            }
            return blocks;
        }
    }

    Program::Program()
        : ops(std::make_shared<const std::vector<Instruction>>()),
          labels(std::make_shared<const label_map_t>()),
          jumps(std::make_shared<const jmp_map_t>()),
          blocks(std::make_shared<const std::vector<Block>>())
    {
    }

    void Program::print(bool without_label) const
    {
        // One extra step so that labels at the end of the program are shown.
        for (instr_ptr_t pc = 0; pc <= this->ops->size(); pc++)
        {
            if (!without_label)
            {
                const auto range = this->labels->equal_range(pc);
                for (auto it = range.first; it != range.second; ++it)
                    std::cout << LABEL_SEPARATOR << it->second << LABEL_SEPARATOR;
            }
            if (pc < this->ops->size())
                std::cout << instr_char((*this->ops)[pc]);
        }
        std::cout << std::endl;
    }

    void Program::run()
    {
        this->memory_model.reset();

        instr_ptr_t ip = 0;
        while (ip < this->ops->size())
        {
            switch ((*this->ops)[ip])
            {
            case Instruction::left:
                this->memory_model.left();
                ip++;
                break;

            case Instruction::right:
                this->memory_model.right();
                ip++;
                break;

            case Instruction::inc:
                this->memory_model.increment();
                ip++;
                break;

            case Instruction::dec:
                this->memory_model.decrement();
                ip++;
                break;

            case Instruction::get:
                if (const std::optional<io_char_t> input = this->io_model.read_next_char())
                    this->memory_model.set_current_value(input.value());
                else if (!this->io_model.is_no_change_on_eof())
                    this->memory_model.set_current_value(this->io_model.get_eof_char());
                ip++;
                break;

            case Instruction::put:
                std::cout.put(static_cast<char>(this->memory_model.get_current_value()));
                ip++;
                break;

            case Instruction::fwd:
                if (this->memory_model.get_current_value() == 0)
                    ip = this->jump_target(ip);
                else
                    ip++;
                break;

            case Instruction::bwd:
                if (this->memory_model.get_current_value() != 0)
                    ip = this->jump_target(ip);
                else
                    ip++;
                break;
            }
        }
    }

    bool Program::has_label(const std::string &label) const
    {
        for (const auto &entry : *this->labels)
            if (entry.second == label)
                return true;
        return false;
    }

    bool Program::has_label_at(instr_ptr_t pc, const std::string &label) const
    {
        const auto range = this->labels->equal_range(pc);
        for (auto entry = range.first; entry != range.second; ++entry)
            if (entry->second == label)
                return true;
        return false;
    }

    std::optional<Instruction> Program::instr_for_pc(instr_ptr_t pc) const
    {
        if (pc < this->ops->size())
            return std::optional<Instruction>{(*this->ops)[pc]};
        return std::nullopt;
    }

    bool Program::is_end(instr_ptr_t pc) const
    {
        return pc >= this->instr_count();
    }

    std::optional<std::string> Program::label_for_instr_ptr(instr_ptr_t pc) const
    {
        const auto it = this->labels->find(pc);
        if (it == this->labels->end())
            return std::nullopt;
        return std::optional<std::string>{it->second};
    }

    instr_ptr_t Program::jump_target(instr_ptr_t pc) const
    {
        return this->jumps->at(pc);
    }

    instr_ptr_t Program::instr_count() const
    {
        return this->ops->size();
    }

    const label_map_t &Program::get_label_map() const
    {
        return *this->labels;
    }

    Block Program::block_at(instr_ptr_t pc) const
    {
        return this->blocks->at(pc);
    }

    instr_ptr_t Program::block_end(instr_ptr_t pc) const
    {
        if (pc >= this->blocks->size())
            return pc;
        return (*this->blocks)[pc].end;
    }

    Program Program::parse_from_file(
        const char *filename, MemoryModel memory_model, IOModel io_model)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            throw ParseException("Could not open file");
        return parse_from_istream(&file, memory_model, io_model);
    }

    Program Program::parse_from_istream(
        std::istream *stream, MemoryModel memory_model, IOModel io_model)
    {
        // Reading the whole source at once is substantially faster than one
        // formatted extraction per character, and the tokenizer below only
        // needs random access.
        const std::string source{
            std::istreambuf_iterator<char>(*stream),
            std::istreambuf_iterator<char>()};

        auto ops = std::make_shared<std::vector<Instruction>>();
        auto labels = std::make_shared<label_map_t>();
        auto jumps = std::make_shared<jmp_map_t>();
        ops->reserve(source.size());
        std::stack<instr_ptr_t> open_brackets;

        for (std::size_t i = 0; i < source.size(); i++)
        {
            // The pointer of the instruction the next opcode gets, or of the
            // label that follows the program text.
            const instr_ptr_t pc = ops->size();

            switch (source[i])
            {
            case '<':
                ops->push_back(Instruction::left);
                break;

            case '>':
                ops->push_back(Instruction::right);
                break;

            case '+':
                ops->push_back(Instruction::inc);
                break;

            case '-':
                ops->push_back(Instruction::dec);
                break;

            case ',':
                ops->push_back(Instruction::get);
                break;

            case '.':
                ops->push_back(Instruction::put);
                break;

            case '[':
                open_brackets.push(pc);
                ops->push_back(Instruction::fwd);
                break;

            case ']':
            {
                if (open_brackets.empty())
                    throw ParseException("Jump instructions are not balanced");
                const instr_ptr_t open = open_brackets.top();
                open_brackets.pop();
                // A fwd jumps behind its bwd when the cell is zero and vice
                // versa; both targets are one past the partner instruction.
                jumps->emplace(open, pc + 1);
                jumps->emplace(pc, open + 1);
                ops->push_back(Instruction::bwd);
                break;
            }

            case LABEL_SEPARATOR:
            {
                const std::size_t end = source.find(LABEL_SEPARATOR, i + 1);
                const std::size_t label_end = end == std::string::npos ? source.size() : end;
                labels->emplace(pc, source.substr(i + 1, label_end - i - 1));
                i = label_end;
                break;
            }

            default:
                break;
            }
        }

        if (!open_brackets.empty())
            throw ParseException("Jump instructions are not balanced");

        auto blocks = build_blocks(*ops, *labels, memory_model);

        Program prog;
        prog.memory_model = memory_model;
        prog.io_model = io_model;
        prog.ops = std::move(ops);
        prog.labels = std::move(labels);
        prog.jumps = std::move(jumps);
        prog.blocks = std::move(blocks);
        return prog;
    }
}
