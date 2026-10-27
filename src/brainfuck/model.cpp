#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include "model.hpp"

namespace brainfuck
{
    MemoryModel::MemoryModel(CellSize cell_size, memory_size_t size, bool wrapping)
        : cell_size(cell_size),
          memory_size(size),
          wrapping(wrapping),
          ptr(0),
          memory()
    {
        if (size == 0)
            throw std::invalid_argument("MemoryModel needs at least one cell");
    }

    MemoryModel::~MemoryModel()
    {
    }

    mem_ptr_t MemoryModel::pointer_left(mem_ptr_t ptr) const
    {
        if (ptr > 0)
            return ptr - 1;
        else if (this->wrapping)
            return this->memory_size - 1;
        else
            return 0;
    }

    mem_ptr_t MemoryModel::pointer_right(mem_ptr_t ptr) const
    {
        if (ptr + 1 < this->memory_size)
            return ptr + 1;
        else if (this->wrapping)
            return 0;
        else
            return this->memory_size - 1;
    }

    cell_value_t MemoryModel::next_value(cell_value_t value) const
    {
        if (value >= this->get_max_value())
            return 0;
        else
            return value + 1;
    }

    cell_value_t MemoryModel::prev_value(cell_value_t value) const
    {
        if (value == 0)
            return this->get_max_value();
        else
            return value - 1;
    }

    cell_value_t MemoryModel::normalize_value(uint64_t value) const
    {
        // Cell sizes are powers of two, so masking is equivalent to a modulo
        // and cannot overflow (get_max_value() + 1 would wrap for 32 bit).
        return static_cast<cell_value_t>(value & this->get_max_value());
    }

    cell_value_t MemoryModel::shift_value(cell_value_t value, int64_t delta) const
    {
        // Converting a negative delta to uint64_t is a modulo 2^64, and every
        // cell width divides 2^64, so masking afterwards stays exact.
        return this->normalize_value(
            static_cast<uint64_t>(value) + static_cast<uint64_t>(delta));
    }

    mem_ptr_t MemoryModel::move_pointer(mem_ptr_t ptr, int64_t net, mem_ptr_t low, mem_ptr_t high) const
    {
        if (this->wrapping)
        {
            const int64_t size = static_cast<int64_t>(this->memory_size);
            int64_t moved = (static_cast<int64_t>(ptr) + net) % size;
            if (moved < 0)
                moved += size;
            return static_cast<mem_ptr_t>(moved);
        }

        // A clamping tape loses track of moves that hit an edge, so a block is
        // not just its net displacement: bounds the block's composition with
        // itself. Both are precomputed once per block (see Program).
        int64_t moved = static_cast<int64_t>(ptr) + net;
        if (moved < static_cast<int64_t>(low))
            moved = static_cast<int64_t>(low);
        else if (moved > static_cast<int64_t>(high))
            moved = static_cast<int64_t>(high);
        return static_cast<mem_ptr_t>(moved);
    }

    void MemoryModel::reset()
    {
        this->ptr = 0;
        this->memory.clear();
    }

    void MemoryModel::left()
    {
        this->ptr = this->pointer_left(this->ptr);
    }

    void MemoryModel::right()
    {
        this->ptr = this->pointer_right(this->ptr);
    }

    void MemoryModel::increment()
    {
        // A single lookup: the cell is created on demand and updated in place.
        cell_value_t &value = this->memory[this->ptr];
        value = this->next_value(value);
    }

    void MemoryModel::decrement()
    {
        cell_value_t &value = this->memory[this->ptr];
        value = this->prev_value(value);
    }

    mem_ptr_t MemoryModel::get_pointer() const
    {
        return this->ptr;
    }

    cell_value_t MemoryModel::get_value(mem_ptr_t ptr) const
    {
        const auto it = this->memory.find(ptr);
        return it == this->memory.end() ? 0 : it->second;
    }

    void MemoryModel::set_value(mem_ptr_t ptr, cell_value_t value)
    {
        this->memory.insert_or_assign(ptr, this->normalize_value(value));
    }

    cell_value_t MemoryModel::get_current_value() const
    {
        return this->get_value(this->ptr);
    }

    void MemoryModel::set_current_value(cell_value_t value)
    {
        this->set_value(this->ptr, value);
    }

    cell_value_t MemoryModel::get_max_value() const
    {
        switch (this->cell_size)
        {
        case CellSize::EightBit:
            return UINT8_MAX;

        case CellSize::SixteenBit:
            return UINT16_MAX;

        case CellSize::ThirtyTwoBit:
            return UINT32_MAX;
        }
        std::abort();
    }

    CellSize MemoryModel::get_cell_size() const
    {
        return this->cell_size;
    }

    memory_size_t MemoryModel::get_memory_size() const
    {
        return this->memory_size;
    }

    bool MemoryModel::is_wrapping() const
    {
        return this->wrapping;
    }

    void MemoryModel::set_cell_size(CellSize cell_size)
    {
        this->cell_size = cell_size;
    }

    void MemoryModel::set_memory_size(memory_size_t size)
    {
        if (size == 0)
            throw std::invalid_argument("MemoryModel needs at least one cell");

        this->memory_size = size;
        if (this->ptr >= size)
            this->ptr = size - 1;
    }

    void MemoryModel::set_wrapping(bool wrapping)
    {
        this->wrapping = wrapping;
    }

    IOModel::IOModel()
        : chars_until_eof(std::nullopt),
          no_change_on_eof(false),
          eof_char(0),
          input(std::nullopt)
    {
    }

    IOModel::IOModel(size_t chars_until_eof)
        : chars_until_eof(std::make_optional(chars_until_eof)),
          no_change_on_eof(false),
          eof_char(0),
          input(std::nullopt)
    {
    }

    std::optional<size_t> IOModel::get_chars_until_eof() const
    {
        return this->chars_until_eof;
    }

    void IOModel::set_chars_until_eof(size_t chars_until_eof)
    {
        this->chars_until_eof = std::make_optional(chars_until_eof);
    }

    void IOModel::set_input(std::string bytes)
    {
        // The budget is the cursor into the pinned input, so it has to start at
        // the input's length; the state's remaining count then names the next
        // byte without carrying the input itself.
        this->chars_until_eof = std::make_optional(bytes.size());
        this->input = std::make_optional(std::move(bytes));
    }

    bool IOModel::has_input() const
    {
        return this->input.has_value();
    }

    const std::optional<std::string> &IOModel::get_input() const
    {
        return this->input;
    }

    size_t IOModel::input_index(size_t chars_left) const
    {
        return this->input.value().size() - chars_left;
    }

    void IOModel::set_eof_char(io_char_t eof_char)
    {
        this->eof_char = eof_char;
    }

    io_char_t IOModel::get_eof_char() const
    {
        return this->eof_char;
    }

    void IOModel::set_no_change_on_eof(bool no_change)
    {
        this->no_change_on_eof = no_change;
    }

    bool IOModel::is_no_change_on_eof() const
    {
        return this->no_change_on_eof;
    }

    std::optional<io_char_t> IOModel::read_next_char()
    {
        if (this->chars_until_eof.has_value() && this->chars_until_eof.value() == 0)
            return std::nullopt;

        if (this->input.has_value())
        {
            // A pinned input never touches the stream, so `execute --input`
            // ignores a piped stdin.
            const std::string &bytes = this->input.value();
            const size_t remaining = this->chars_until_eof.has_value()
                                         ? std::min(this->chars_until_eof.value(), bytes.size())
                                         : bytes.size();
            if (remaining == 0)
                return std::nullopt;

            const io_char_t character = static_cast<io_char_t>(bytes[this->input_index(remaining)]);
            this->chars_until_eof = std::make_optional(remaining - 1);
            return character;
        }

        // std::cin >> c would skip whitespace, which would silently drop
        // space, tab and newline input characters.
        const int next_byte = std::cin.get();
        if (next_byte == std::char_traits<char>::eof())
            return std::nullopt;

        if (this->chars_until_eof.has_value())
            this->chars_until_eof = this->chars_until_eof.value() - 1;

        return static_cast<io_char_t>(next_byte);
    }

    //! Builds the alphabet `0..255` once, so every read can share it.
    static std::vector<io_char_t> make_all_possible_chars()
    {
        std::vector<io_char_t> chars;
        chars.reserve(static_cast<size_t>(std::numeric_limits<io_char_t>::max()) + 1);
        for (unsigned int c = 0; c <= std::numeric_limits<io_char_t>::max(); c++)
            chars.push_back(static_cast<io_char_t>(c));
        return chars;
    }

    //! Shared alphabet of every character a read can deliver; never modified.
    static const std::vector<io_char_t> all_possible_chars = make_all_possible_chars();

    std::vector<io_char_t> IOModel::read_possible_chars()
    {
        if (this->input.has_value())
        {
            const std::string &bytes = this->input.value();
            const size_t remaining = this->chars_until_eof.has_value()
                                         ? std::min(this->chars_until_eof.value(), bytes.size())
                                         : bytes.size();

            if (remaining > 0)
            {
                this->chars_until_eof = std::make_optional(remaining - 1);
                return std::vector<io_char_t>{
                    static_cast<io_char_t>(bytes[this->input_index(remaining)])};
            }
            else if (this->no_change_on_eof)
            {
                return std::vector<io_char_t>{};
            }
            else
            {
                return std::vector<io_char_t>{this->eof_char};
            }
        }

        if (!this->chars_until_eof.has_value())
            return all_possible_chars;

        const size_t chars_left = this->chars_until_eof.value();
        if (chars_left > 0)
        {
            this->chars_until_eof = std::make_optional(chars_left - 1);
            return all_possible_chars;
        }
        else if (this->no_change_on_eof)
        {
            return std::vector<io_char_t>{};
        }
        else
        {
            return std::vector<io_char_t>{this->eof_char};
        }
    }

    std::vector<io_char_t> IOModel::get_possible_chars(std::optional<size_t> chars_left) const
    {
        if (this->input.has_value())
        {
            const std::string &bytes = this->input.value();
            const size_t remaining = chars_left.has_value()
                                         ? std::min(chars_left.value(), bytes.size())
                                         : bytes.size();

            if (remaining > 0)
                return std::vector<io_char_t>{
                    static_cast<io_char_t>(bytes[this->input_index(remaining)])};
            else if (this->no_change_on_eof)
                return std::vector<io_char_t>{};
            else
                return std::vector<io_char_t>{this->eof_char};
        }

        const size_t remaining = chars_left.has_value() ? chars_left.value() : 1;

        if (remaining > 0)
            return all_possible_chars;
        else if (this->no_change_on_eof)
            return std::vector<io_char_t>{};
        else
            return std::vector<io_char_t>{this->eof_char};
    }

    namespace
    {
        //! Value of \a c as a hexadecimal digit, or -1 when it is not one.
        int hex_digit_value(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }
    }

    std::string decode_input_escapes(const std::string &text)
    {
        std::string decoded;
        decoded.reserve(text.size());

        for (size_t i = 0; i < text.size(); i++)
        {
            if (text[i] != '\\')
            {
                decoded.push_back(text[i]);
                continue;
            }

            if (i + 1 >= text.size())
                throw std::invalid_argument("input ends with a lone backslash");

            const char escape = text[++i];
            switch (escape)
            {
            case '\\':
                decoded.push_back('\\');
                break;
            case 'n':
                decoded.push_back('\n');
                break;
            case 't':
                decoded.push_back('\t');
                break;
            case 'r':
                decoded.push_back('\r');
                break;
            case '0':
                decoded.push_back('\0');
                break;
            case 'x':
            {
                // Exactly two digits keep `\x41bc` unambiguous: it is `A` + "bc".
                if (i + 2 >= text.size())
                    throw std::invalid_argument("\\x needs exactly two hexadecimal digits");

                const int high = hex_digit_value(text[i + 1]);
                const int low = hex_digit_value(text[i + 2]);
                if (high < 0 || low < 0)
                    throw std::invalid_argument("\\x needs exactly two hexadecimal digits");

                decoded.push_back(static_cast<char>(high * 16 + low));
                i += 2;
                break;
            }
            default:
                throw std::invalid_argument(std::string("unknown escape \\") + escape);
            }
        }

        return decoded;
    }
}
