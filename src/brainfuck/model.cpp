#include <iostream>
#include "model.hpp"

namespace brainfuck
{
    MemoryModel::MemoryModel(CellSize cell_size, memory_size_t size, bool wrapping)
    {
        this->cell_size = cell_size;
        this->memory_size = size;
        this->wrapping = wrapping;
        this->ptr = 0;
        this->memory = std::map<mem_ptr_t, uint32_t>();
    }

    MemoryModel::~MemoryModel()
    {
    }

    void MemoryModel::reset()
    {
        this->ptr = 0;
        this->memory = std::map<mem_ptr_t, uint32_t>();
    }

    void MemoryModel::left()
    {
        if (this->ptr > 0)
            this->ptr--;
        else if (this->wrapping)
            this->ptr = this->memory_size - 1;
        else
            this->ptr = 0;
    }

    void MemoryModel::right()
    {
        if (this->ptr < this->memory_size - 1)
            this->ptr++;
        else if (this->wrapping)
            this->ptr = 0;
        else
            this->ptr = this->memory_size - 1;
    }

    void MemoryModel::increment()
    {
        uint32_t value = this->get_value(this->ptr);
        if (value == this->get_max_value())
            this->memory[this->ptr] = 0;
        else
            this->memory[this->ptr] = value + 1;
    }

    void MemoryModel::decrement()
    {
        uint32_t value = this->get_value(this->ptr);
        if (value == 0)
            this->memory[this->ptr] = this->get_max_value();
        else
            this->memory[this->ptr] = value - 1;
    }

    mem_ptr_t MemoryModel::get_pointer()
    {
        return this->ptr;
    }

    uint32_t MemoryModel::get_value(mem_ptr_t ptr)
    {
        if (this->memory.count(ptr) > 0)
            return this->memory[ptr];
        else
            return 0;
    }

    void MemoryModel::set_value(mem_ptr_t ptr, uint8_t value)
    {
        this->memory[ptr] = (uint32_t)value;
    }

    void MemoryModel::set_value(mem_ptr_t ptr, uint32_t value)
    {
        if (this->cell_size == CellSize::EightBit)
            value %= UINT8_MAX + 1;
        else if (this->cell_size == CellSize::SixteenBit)
            value %= UINT16_MAX + 1;
        this->memory[ptr] = value;
    }

    uint32_t MemoryModel::get_current_value()
    {
        return this->get_value(this->ptr);
    }

    void MemoryModel::set_current_value(uint32_t value)
    {
        return this->set_value(this->ptr, value);
    }

    uint32_t MemoryModel::get_max_value() const
    {
        switch (this->cell_size)
        {
        case CellSize::EightBit:
            return UINT8_MAX;

        case CellSize::SixteenBit:
            return UINT16_MAX;

        case CellSize::ThirtyTwoBit:
            return UINT32_MAX;

        default:
            abort();
        }
    }

    IOModel::IOModel()
    {
        this->chars_until_eof = std::nullopt;
        this->eof_char = 0;
        this->no_change_on_eof = false;
    }

    IOModel::IOModel(size_t chars_until_eof)
    {
        this->chars_until_eof = std::make_optional(chars_until_eof);
        this->eof_char = 0;
        this->no_change_on_eof = false;
    }

    std::optional<size_t> IOModel::get_chars_until_eof() const
    {
        return this->chars_until_eof;
    }

    void IOModel::set_eof_char(uint8_t eof_char)
    {
        this->eof_char = eof_char;
    }

    void IOModel::set_no_change_on_eof(bool no_change)
    {
        this->no_change_on_eof = no_change;
    }

    void IOModel::set_chars_until_eof(size_t chars_until_eof)
    {
        this->chars_until_eof = std::make_optional(chars_until_eof);
    }

    uint8_t IOModel::read_next_char()
    {
        char in;
        std::cin >> in;
        if (std::cin.eof())
            in = 0;
        return (uint8_t)in;
    }

    static const std::vector<uint8_t>
        all_possible_chars{0, 1, 2, 3, 4, 5, 6, 7,
                           8, 9, 10, 11, 12, 13, 14, 15,
                           16, 17, 18, 19, 20, 21, 22, 23,
                           24, 25, 26, 27, 28, 29, 30, 31,
                           32, 33, 34, 35, 36, 37, 38, 39,
                           40, 41, 42, 43, 44, 45, 46, 47,
                           48, 49, 50, 51, 52, 53, 54, 55,
                           56, 57, 58, 59, 60, 61, 62, 63,
                           64, 65, 66, 67, 68, 69, 70, 71,
                           72, 73, 74, 75, 76, 77, 78, 79,
                           80, 81, 82, 83, 84, 85, 86, 87,
                           88, 89, 90, 91, 92, 93, 94, 95,
                           96, 97, 98, 99, 100, 101, 102, 103,
                           104, 105, 106, 107, 108, 109, 110, 111,
                           112, 113, 114, 115, 116, 117, 118, 119,
                           120, 121, 122, 123, 124, 125, 126, 127,
                           128, 129, 130, 131, 132, 133, 134, 135,
                           136, 137, 138, 139, 140, 141, 142, 143,
                           144, 145, 146, 147, 148, 149, 150, 151,
                           152, 153, 154, 155, 156, 157, 158, 159,
                           160, 161, 162, 163, 164, 165, 166, 167,
                           168, 169, 170, 171, 172, 173, 174, 175,
                           176, 177, 178, 179, 180, 181, 182, 183,
                           184, 185, 186, 187, 188, 189, 190, 191,
                           192, 193, 194, 195, 196, 197, 198, 199,
                           200, 201, 202, 203, 204, 205, 206, 207,
                           208, 209, 210, 211, 212, 213, 214, 215,
                           216, 217, 218, 219, 220, 221, 222, 223,
                           224, 225, 226, 227, 228, 229, 230, 231,
                           232, 233, 234, 235, 236, 237, 238, 239,
                           240, 241, 242, 243, 244, 245, 246, 247,
                           248, 249, 250, 251, 252, 253, 254, 255};

    std::vector<uint8_t> IOModel::read_possible_chars()
    {
        if (!this->chars_until_eof.has_value())
            return all_possible_chars;
        size_t chars_left = this->chars_until_eof.value();
        if (chars_left > 0)
        {
            this->chars_until_eof = std::make_optional(chars_left - 1);
            return all_possible_chars;
        }
        else if (this->no_change_on_eof)
        {
            return std::vector<uint8_t>{};
        }
        else
        {
            return std::vector<uint8_t>{this->eof_char};
        }
    }

    std::vector<uint8_t> IOModel::get_possible_chars(std::optional<size_t> m_chars_left)
    {
        size_t chars_left = 1;
        if (m_chars_left.has_value())
            chars_left = m_chars_left.value();

        if (chars_left > 0)
        {
            return all_possible_chars;
        }
        else if (this->no_change_on_eof)
        {
            return std::vector<uint8_t>{};
        }
        else
        {
            return std::vector<uint8_t>{this->eof_char};
        }
    }
}