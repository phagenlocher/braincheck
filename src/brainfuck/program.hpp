#pragma once

#include <map>
#include <vector>
#include <iostream>
#include <optional>
#include "model.hpp"

namespace brainfuck
{
    typedef unsigned long instr_ptr_t;

    const char LABEL_SEPARATOR = '_';

    enum Instruction
    {
        left,  // <
        right, // >
        inc,   // +
        dec,   // -
        get,   // ,
        put,   // .
        fwd,   // [
        bwd    // ]
    };

    char instr_char(Instruction);

    class Program
    {
    private:
        std::vector<Instruction> ops;
        std::map<instr_ptr_t, std::string> label_map;
        std::map<instr_ptr_t, instr_ptr_t> jmp_map;
        void build_jmp_map();

    public:
        Program();
        MemoryModel memory_model;
        IOModel io_model;
        void print(bool without_label = false);
        void run();
        bool has_label(std::string label);
        std::optional<Instruction> instr_for_pc(instr_ptr_t pc);
        std::optional<std::string> label_for_instr_ptr(instr_ptr_t ip);
        std::map<instr_ptr_t, std::string> get_label_map();
        std::map<instr_ptr_t, instr_ptr_t> get_jmp_map();
        static Program parse_from_file(const char *filename, MemoryModel memory_model, IOModel io_model);
        static Program parse_from_istream(std::istream *stream, MemoryModel memory_model, IOModel io_model);
    };

    class ParseException : public std::exception
    {
    private:
        std::string message;

    public:
        ParseException(std::string msg) : message(msg) {}
        const char *what()
        {
            return message.c_str();
        }
    };
}