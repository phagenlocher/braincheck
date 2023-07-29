#include <map>
#include <stack>
#include <vector>
#include <utility>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdlib.h>
#include "program.hpp"

using namespace std;

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
        default:
            abort();
        }
    }

    Program::Program()
    {
    }

    void Program::build_jmp_map()
    {
        stack<instr_ptr_t> fwd_stack;
        instr_ptr_t old_ip, ip = 0;

        for (const Instruction &op : this->ops)
        {
            if (op == Instruction::fwd)
            {
                fwd_stack.push(ip);
            }
            else if (op == Instruction::bwd)
            {
                old_ip = fwd_stack.top();
                this->jmp_map.insert(make_pair(ip, old_ip + 1));
                this->jmp_map.insert(make_pair(old_ip, ip + 1));
                fwd_stack.pop();
            }
            ip++;
        }
    }

    void Program::print(bool without_label)
    {
        instr_ptr_t pc = 0;
        for (const Instruction &op : this->ops)
        {
            if ((!without_label) && this->label_map.count(pc) > 0)
            {
                cout
                    << LABEL_SEPARATOR
                    << this->label_map[pc]
                    << LABEL_SEPARATOR;
            }
            cout << instr_char(op);
            pc++;
        }
        cout << endl;
    }

    void Program::run()
    {
        this->memory_model.reset();
        instr_ptr_t ip = 0;
        uint8_t in;

        while (ip < this->ops.size())
        {
            switch (this->ops[ip])
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
                in = this->io_model.read_next_char();
                this->memory_model.set_current_value(in);
                ip++;
                break;

            case Instruction::put:
                cout << ((char)this->memory_model.get_current_value());
                ip++;
                break;

            case Instruction::fwd:
                if (this->memory_model.get_current_value() == 0)
                    ip = this->jmp_map.at(ip);
                else
                    ip++;
                break;

            case Instruction::bwd:
                if (this->memory_model.get_current_value() != 0)
                    ip = this->jmp_map.at(ip);
                else
                    ip++;
                break;

            default:
                abort();
            }
        }
    }

    bool Program::has_label(string label)
    {
        for (const auto &kv : this->label_map)
        {
            if (kv.second.compare(label) == 0)
                return true;
        }
        return false;
    }

    std::optional<Instruction> Program::instr_for_pc(instr_ptr_t pc)
    {
        if (pc < this->ops.size())
            return std::optional<Instruction>{this->ops[pc]};
        else
            return std::nullopt;
    }

    std::optional<std::string> Program::label_for_instr_ptr(instr_ptr_t ip)
    {
        try
        {
            auto label = this->label_map.at(ip);
            return std::optional<std::string>{label};
        }
        catch (std::out_of_range &e)
        {
            return std::nullopt;
        }
    }

    map<instr_ptr_t, string> Program::get_label_map()
    {
        return this->label_map;
    }

    map<instr_ptr_t, instr_ptr_t> Program::get_jmp_map()
    {
        return this->jmp_map;
    }

    Program Program::parse_from_file(
        const char *filename, MemoryModel memory_model, IOModel io_model)
    {
        ifstream file;
        file.open(filename);
        if (!file.is_open())
        {
            throw ParseException("Could not open file");
        }
        Program prog = parse_from_istream(&file, memory_model, io_model);
        file.close();
        return prog;
    }

    Program Program::parse_from_istream(
        istream *stream, MemoryModel memory_model, IOModel io_model)
    {
        Program prog;
        vector<Instruction> ops;
        map<instr_ptr_t, string> labels;
        int ic;
        char c;
        int jump_counter = 0;
        instr_ptr_t pc = 0;

        prog.memory_model = memory_model;
        prog.io_model = io_model;

        while ((ic = stream->get()) != EOF)
        {
            c = (char)ic;
            bool reading_label = false;
            switch (c)
            {
            case '<':
                pc++;
                ops.push_back(left);
                break;

            case '>':
                pc++;
                ops.push_back(right);
                break;

            case '+':
                pc++;
                ops.push_back(inc);
                break;

            case '-':
                pc++;
                ops.push_back(dec);
                break;

            case ',':
                pc++;
                ops.push_back(get);
                break;

            case '.':
                pc++;
                ops.push_back(put);
                break;

            case '[':
                pc++;
                jump_counter += 1;
                ops.push_back(fwd);
                break;

            case ']':
                pc++;
                jump_counter -= 1;
                ops.push_back(bwd);
                break;

            case LABEL_SEPARATOR:
                reading_label = true;
                break;

            default:
                break;
            }

            if (jump_counter < 0)
            {
                throw ParseException("Jump instructions are not balanced");
            }

            // We have started reading a label, so we read the rest
            if (reading_label)
            {
                string label;
                while ((ic = stream->get()) != EOF)
                {
                    c = (char)ic;
                    if (c == LABEL_SEPARATOR)
                    {
                        break;
                    }
                    label.push_back(c);
                }
                labels.insert(make_pair(pc, label));
            }
        }

        if (jump_counter != 0)
        {
            throw ParseException("Jump instructions are not balanced");
        }

        prog.ops = ops;
        prog.label_map = labels;
        prog.build_jmp_map();
        return prog;
    }
}