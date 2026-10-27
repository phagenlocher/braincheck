#pragma once

#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <iosfwd>
#include <optional>
#include <utility>
#include "model.hpp"

namespace brainfuck
{
    //! Index into a program's instruction list.
    typedef unsigned long instr_ptr_t;

    //! Character that opens and closes an inline label (`_name_`).
    const char LABEL_SEPARATOR = '_';

    //! The eight Brainfuck opcodes, in source-character order.
    /**
     * The enum value order is an implementation detail; map to and from source
     * characters through \c instr_char() rather than relying on it.
     */
    enum Instruction
    {
        left,  //!< `<`, move the pointer left
        right, //!< `>`, move the pointer right
        inc,   //!< `+`, increment the current cell
        dec,   //!< `-`, decrement the current cell
        get,   //!< `,`, read one character into the current cell
        put,   //!< `.`, write the current cell as one character
        fwd,   //!< `[`, jump behind the matching `]` when the cell is zero
        bwd    //!< `]`, jump behind the matching `[` when the cell is not zero
    };

    //! Source character of \a instr; this is the inverse of parsing.
    char instr_char(Instruction instr);

    //! Class of a straight-line block.
    enum class BlockKind
    {
        single,  //!< one instruction that is not folded
        values,  //!< a run of + and - (net value change)
        pointers //!< a run of > and < (net pointer movement)
    };

    //! A straight-line block starting at one instruction pointer.
    /**
     * Blocks are the states of the folded Kripke structure: consecutive + and -
     * (or > and <) instructions collapse into a single transition. A block
     * never crosses a label and never contains , . [ or ], so every jump target
     * and every successor pointer is the start of a block (see
     * `spec/state-space.md`).
     */
    struct Block
    {
        BlockKind kind;  //!< what the block contains
        instr_ptr_t end; //!< pc after the last instruction of the block
        int64_t delta;   //!< net increments, or net right moves
        mem_ptr_t low;   //!< pointer after the block from cell 0 (clamping tape)
        mem_ptr_t high;  //!< pointer after the block from the last cell
    };

    //! Labels by instruction pointer. Several labels may share a pointer.
    typedef std::multimap<instr_ptr_t, std::string> label_map_t;
    //! Jump targets of the bracket instructions, keyed by their pointer.
    typedef std::map<instr_ptr_t, instr_ptr_t> jmp_map_t;

    //! A parsed Brainfuck program plus the models it is interpreted under.
    /**
     * The instruction list, labels and jump targets are immutable once
     * parsing is done, so they live behind shared pointers: copying a Program
     * is O(1). That matters because the model checker hands a Program to
     * every successor iterator, and a deep copy there would be paid once per
     * explored state.
     */
    class Program
    {
    private:
        //! Parsed instruction list; shared and immutable after parsing.
        std::shared_ptr<const std::vector<Instruction>> ops;
        //! Inline labels keyed by instruction pointer; shared and immutable.
        std::shared_ptr<const label_map_t> labels;
        //! Bracket jump targets; shared and immutable.
        std::shared_ptr<const jmp_map_t> jumps;
        //! Straight-line block table; shared and immutable.
        std::shared_ptr<const std::vector<Block>> blocks;

    public:
        //! Empty program under default models; parsing fills it in.
        Program();
        //! Memory configuration the interpreter and the Kripke layer use.
        MemoryModel memory_model;
        //! Standard-input configuration the interpreter and the Kripke layer use.
        IOModel io_model;

        //! Writes the program to stdout, one source line, with inline labels.
        /**
         * \param without_label omit the `_label_` markers from the output
         *
         * A label behind the last instruction is printed as well, so `print`
         * round-trips a source whose final label marks the halted state.
         */
        void print(bool without_label = false) const;
        //! Interprets the program to completion, using \c memory_model and \c io_model.
        /**
         * Writes `put` output to stdout and consumes stdin through the IO model.
         * The memory model is reset first, so a second \c run() starts from a
         * clean tape. Execution is unbounded: a non-terminating program does
         * not return.
         */
        void run();

        //! True when \a label occurs anywhere in the program, at any pointer.
        bool has_label(const std::string &label) const;
        //! Instruction at \a pc, or no value at the end point and beyond.
        std::optional<Instruction> instr_for_pc(instr_ptr_t pc) const;
        //! True for the end point, i.e. for a pointer behind the last instruction.
        /**
         * The end point is a state of the Kripke structure: labels written after
         * the last instruction live there. It has no instruction, so the
         * interpreter stops and the model checker stutters (see
         * `spec/state-space.md`).
         */
        bool is_end(instr_ptr_t pc) const;
        //! One label at \a pc, or no value when the pointer carries none.
        /**
         * Several labels may share a pointer; this returns the first one stored
         * for it. Use \c get_label_map() to see all of them.
         */
        std::optional<std::string> label_for_instr_ptr(instr_ptr_t pc) const;
        //! Successor of a bracket instruction; only valid for fwd and bwd.
        /**
         * \pre \a pc indexes a \c fwd or \c bwd instruction (see
         *      \c parse_from_istream())
         * \return the instruction pointer one past the matching bracket
         * \throws std::out_of_range when \a pc is not a bracket
         */
        instr_ptr_t jump_target(instr_ptr_t pc) const;
        //! Number of instructions; also the pointer of the end point.
        instr_ptr_t instr_count() const;
        //! All labels, keyed by instruction pointer.
        const label_map_t &get_label_map() const;

        //! The straight-line block that covers \a pc.
        /**
         * Valid for every pc below `instr_count()`. Every pc of a folded run
         * maps to the same block, so this is a plain lookup; a state always
         * passes a block start. The table is built once while parsing and is
         * immutable and shared, like the instruction list itself.
         *
         * \pre `pc < instr_count()`
         * \throws std::out_of_range otherwise
         */
        Block block_at(instr_ptr_t pc) const;
        //! pc after the block that covers \a pc.
        /**
         * `block_end(instr_count())` is `instr_count()` itself, so callers may
         * use it for the end point without a special case.
         */
        instr_ptr_t block_end(instr_ptr_t pc) const;

        //! Parses \a filename and builds the block table for \a memory_model.
        /**
         * \throws ParseException when the file cannot be opened or the brackets
         *         are not balanced
         *
         * The models are stored in the returned program and also fix the block
         * table, so configure them before parsing and do not mutate them later.
         */
        static Program parse_from_file(const char *filename, MemoryModel memory_model, IOModel io_model);
        //! Parses the whole stream and builds the block table for \a memory_model.
        /**
         * Every `+ - < > . , [ ]` character is an instruction; all other
         * characters outside an inline label are ignored. A label is written
         * as `_name_` and marks the instruction pointer of the next character.
         *
         * \throws ParseException when a `]` has no matching `[` or a `[` is
         *         still open at the end of the source
         */
        static Program parse_from_istream(std::istream *stream, MemoryModel memory_model, IOModel io_model);
    };

    //! Error raised for a source file that cannot be parsed.
    class ParseException : public std::exception
    {
    private:
        //! Keeps the base signature visible even though what() hides it below.
        using std::exception::what;
        //! Human-readable description, returned by \c what().
        std::string message;

    public:
        //! Stores \a msg as the exception's description.
        ParseException(std::string msg) : message(std::move(msg)) {}
        //! Description of the parse failure; valid while this object lives.
        const char *what()
        {
            return message.c_str();
        }
    };
}
