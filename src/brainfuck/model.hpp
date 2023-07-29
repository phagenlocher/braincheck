#pragma once

#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <optional>

namespace brainfuck
{
    typedef unsigned long mem_ptr_t;
    typedef unsigned long memory_size_t;

    enum CellSize
    {
        EightBit,
        SixteenBit,
        ThirtyTwoBit
    };

    class MemoryModel
    {
    private:
        CellSize cell_size;
        memory_size_t memory_size;
        bool wrapping;
        mem_ptr_t ptr;
        std::map<mem_ptr_t, uint32_t> memory;

    public:
        MemoryModel(CellSize cell_size = EightBit, memory_size_t size = 30000, bool wrapping = true);
        ~MemoryModel();
        void reset();
        void left();
        void right();
        void increment();
        void decrement();
        mem_ptr_t get_pointer();
        uint32_t get_value(mem_ptr_t ptr);
        void set_value(mem_ptr_t ptr, uint8_t value);
        void set_value(mem_ptr_t ptr, uint32_t value);
        uint32_t get_current_value();
        void set_current_value(uint32_t value);
        uint32_t get_max_value() const;
    };

    class IOModel
    {
    private:
        std::optional<size_t> chars_until_eof;
        bool no_change_on_eof;
        uint8_t eof_char;

    public:
        IOModel();
        IOModel(size_t chars_until_eof);
        std::optional<size_t> get_chars_until_eof() const;
        void set_eof_char(uint8_t eof_char);
        void set_no_change_on_eof(bool no_change);
        void set_chars_until_eof(size_t chars_until_eof);
        uint8_t read_next_char();
        std::vector<uint8_t> read_possible_chars();
        std::vector<uint8_t> get_possible_chars(std::optional<size_t> chars_left);
    };
}