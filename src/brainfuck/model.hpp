#pragma once

#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <optional>

namespace brainfuck
{
    //! Index of a cell on the memory tape.
    typedef unsigned long mem_ptr_t;
    //! Number of cells on the memory tape.
    typedef unsigned long memory_size_t;
    //! Widest value a single memory cell can hold.
    typedef uint32_t cell_value_t;
    //! A value read from stdin or written to stdout.
    typedef uint8_t io_char_t;

    //! Width of one memory cell in bits.
    enum CellSize
    {
        EightBit,    //!< values 0..255
        SixteenBit,  //!< values 0..65535
        ThirtyTwoBit //!< values 0..4294967295
    };

    // Defaults for the memory model. Everything else must ask the model.
    //! Default cell width.
    const CellSize DEFAULT_CELL_SIZE = CellSize::EightBit;
    //! Default number of cells on the tape.
    const memory_size_t DEFAULT_MEMORY_SIZE = 30000;
    //! Default tape behaviour: wrap around at the edges.
    const bool DEFAULT_WRAPPING = true;

    //! Configuration and mutable state of one interpreted tape.
    /**
     * The model is the single source of truth for cell width, tape size,
     * pointer wrapping and cell normalisation. The interpreter
     * (`Program::run`) and the model checker (`KIterator`) both go through the
     * pure transition helpers below, so neither may hardcode a modulus or a
     * tape width.
     *
     * The mutable part (pointer and sparse cell map) is used by the
     * interpreter only; the Kripke layer keeps memory in a `MemoryMap` and
     * calls just the pure helpers. Cells that were never written read as zero
     * and are not stored.
     */
    class MemoryModel
    {
    private:
        //! Width of one cell in bits.
        CellSize cell_size;
        //! Number of cells on the tape; at least one.
        memory_size_t memory_size;
        //! Whether the pointer wraps at the tape edges instead of clamping.
        bool wrapping;
        //! Current tape position of the interpreter.
        mem_ptr_t ptr;
        //! Sparse interpreter memory; absent cells read as zero.
        std::map<mem_ptr_t, cell_value_t> memory;

    public:
        //! Builds a model with a clean tape and the pointer on cell 0.
        /**
         * \param cell_size width of one cell in bits
         * \param size number of cells on the tape
         * \param wrapping whether the pointer wraps at the edges
         * \throws std::invalid_argument when \a size is zero
         */
        MemoryModel(CellSize cell_size = DEFAULT_CELL_SIZE,
                    memory_size_t size = DEFAULT_MEMORY_SIZE,
                    bool wrapping = DEFAULT_WRAPPING);
        //! Destroys the model and the sparse interpreter tape it owns.
        ~MemoryModel();

        // Pure transition functions. They are the single definition of the
        // memory semantics, shared by the interpreter (Program::run) and the
        // model checker (KIterator).

        //! Pointer after one `<` from \a ptr (wraps or clamps according to the model).
        mem_ptr_t pointer_left(mem_ptr_t ptr) const;
        //! Pointer after one `>` from \a ptr (wraps or clamps according to the model).
        mem_ptr_t pointer_right(mem_ptr_t ptr) const;
        //! Cell value after one `+` from \a value, wrapping at the cell width.
        cell_value_t next_value(cell_value_t value) const;
        //! Cell value after one `-` from \a value, wrapping at the cell width.
        cell_value_t prev_value(cell_value_t value) const;
        //! \a value reduced to the cell width; this is the canonical stored form.
        cell_value_t normalize_value(uint64_t value) const;
        //! Cell value after a whole block of increments and decrements.
        /**
         * \a delta is the net number of increments (negative for net
         * decrements). Cell values always wrap at the cell width, independently
         * of the pointer's wrapping flag.
         */
        cell_value_t shift_value(cell_value_t value, int64_t delta) const;
        //! Pointer after a whole block of moves.
        /**
         * \a net is the signed number of right minus left moves in the block.
         * \a low and \a high are the pointer positions the block reaches from
         * cell 0 and from the last cell; a clamping tape stays between them.
         * A wrapping tape only needs \a net and ignores the bounds.
         */
        mem_ptr_t move_pointer(mem_ptr_t ptr, int64_t net, mem_ptr_t low, mem_ptr_t high) const;

        //! Clears every cell and puts the pointer back on cell 0.
        void reset();
        //! Moves the interpreter pointer one cell left.
        void left();
        //! Moves the interpreter pointer one cell right.
        void right();
        //! Increments the current cell, wrapping at the cell width.
        void increment();
        //! Decrements the current cell, wrapping at the cell width.
        void decrement();
        //! Current interpreter tape position.
        mem_ptr_t get_pointer() const;
        //! Value of \a ptr, or zero when the cell was never written.
        cell_value_t get_value(mem_ptr_t ptr) const;
        //! Sets \a ptr to \a value, reduced to the cell width.
        void set_value(mem_ptr_t ptr, cell_value_t value);
        //! Value of the cell under the interpreter pointer.
        cell_value_t get_current_value() const;
        //! Sets the cell under the interpreter pointer, reduced to the cell width.
        void set_current_value(cell_value_t value);
        //! Largest value a cell can hold for the configured cell size.
        cell_value_t get_max_value() const;

        //! Configured cell width.
        CellSize get_cell_size() const;
        //! Configured number of cells.
        memory_size_t get_memory_size() const;
        //! Whether the pointer wraps instead of clamping.
        bool is_wrapping() const;
        //! Changes the cell width; does not rewrite already stored values.
        void set_cell_size(CellSize cell_size);
        //! Changes the tape size, clamping the pointer into the new tape.
        /**
         * \throws std::invalid_argument when \a size is zero
         */
        void set_memory_size(memory_size_t size);
        //! Selects wrapping or clamping pointer behaviour.
        void set_wrapping(bool wrapping);
    };

    //! Configuration of standard input for the interpreter and the model checker.
    /**
     * The model describes which characters a read can deliver and when the
     * input ends. It holds the end-of-input policy, an optional length budget
     * and an optional concrete input; it does not own the stream, and reading
     * happens through `std::cin` in the interpreter (see `spec/input.md`).
     *
     * Without a concrete input a read may deliver any of the 256 byte values
     * (symbolic input). With one, a read is deterministic: it delivers the
     * next byte of \c input, which is what `--input` pins for the analysis.
     */
    class IOModel
    {
    private:
        //! Reads left before the input ends, or no value for an unbounded input.
        std::optional<size_t> chars_until_eof;
        //! Whether an exhausted input leaves the cell unchanged instead of writing the EOF char.
        bool no_change_on_eof;
        //! Character written when the input is exhausted and the cell does change.
        io_char_t eof_char;
        //! Concrete input bytes, or no value for a symbolic input.
        /**
         * The bytes are fixed for one exploration, so the state's remaining
         * budget identifies which byte comes next: nothing about the input has
         * to be carried per state (see `spec/state-space.md`).
         */
        std::optional<std::string> input;

        //! Index of the next byte of \c input when \a chars_left remain.
        /**
         * \pre \c input has a value and \a chars_left is at most its length
         */
        size_t input_index(size_t chars_left) const;

    public:
        //! Unbounded input, no change on EOF and a zero EOF character.
        IOModel();
        //! Input of at most \a chars_until_eof characters, then end of input.
        IOModel(size_t chars_until_eof);
        //! Configured remaining input length, or no value for an unbounded input.
        std::optional<size_t> get_chars_until_eof() const;
        //! Bounds the input to \a chars_until_eof characters from now on.
        /**
         * Only for a symbolic input. Pinning an input already fixes how many
         * bytes are readable, so do not call this after \c set_input().
         */
        void set_chars_until_eof(size_t chars_until_eof);
        //! Pins the input to \a bytes, so every read is deterministic.
        /**
         * Replaces any length budget with the length of \a bytes: the budget is
         * the cursor into the pinned input, and the state's remaining budget
         * identifies the next byte. Set the end-of-input policy with
         * \c set_eof_char() and \c set_no_change_on_eof() as usual; it applies
         * to every read behind the last byte.
         */
        void set_input(std::string bytes);
        //! True when the input is pinned to concrete bytes.
        bool has_input() const;
        //! The pinned input bytes, or no value for a symbolic input.
        const std::optional<std::string> &get_input() const;
        //! Character delivered when the input is exhausted.
        void set_eof_char(io_char_t eof_char);
        //! Character delivered when the input is exhausted.
        io_char_t get_eof_char() const;
        //! Selects whether an exhausted input leaves the cell unchanged.
        void set_no_change_on_eof(bool no_change);
        //! True when an exhausted input must leave the cell unchanged.
        bool is_no_change_on_eof() const;
        //! Next character from stdin, or no value once the input is exhausted.
        /**
         * Consumes one character of the configured length budget, if there is
         * one. The caller decides what to do at the end of the input, because
         * that depends on the program's cell (see Program::run).
         *
         * With a pinned input the next byte is returned and `std::cin` is not
         * touched at all.
         */
        std::optional<io_char_t> read_next_char();
        //! All characters the next read can deliver; advances the model.
        /**
         * Consumes one character of the configured length budget, exactly like
         * \c read_next_char(). An empty result means the read must leave the
         * cell unchanged; a single-element result means the read is
         * deterministic.
         */
        std::vector<io_char_t> read_possible_chars();
        //! All characters a read can deliver when \a chars_left remain.
        /**
         * Pure counterpart of \c read_possible_chars(): the budget is not
         * modified. An absent budget means unbounded input. With a pinned input
         * the result is exactly the next byte, so the read is deterministic.
         */
        std::vector<io_char_t> get_possible_chars(std::optional<size_t> chars_left) const;
    };

    //! Decodes the escape syntax of the command line's `--input` value.
    /**
     * Supported escapes are `\\\\`, `\\n`, `\\t`, `\\r`, `\\0` and `\\xNN` with
     * exactly two hexadecimal digits, case-insensitively. Every escape becomes
     * one byte, so `\\x00` expresses a NUL byte that an argument vector cannot
     * carry. All other characters are copied verbatim.
     *
     * \param text the raw argument value
     * \return the decoded bytes
     * \throws std::invalid_argument when an escape is unknown or `\\x` is not
     *         followed by exactly two hexadecimal digits
     */
    std::string decode_input_escapes(const std::string &text);
}
