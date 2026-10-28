#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdlib.h>
#include <CLI/CLI.hpp>
#include <brainfuck.hpp>
#include "argparse.hpp"

namespace bf = brainfuck;

namespace argparse
{
    namespace
    {
        //! Raw values of the memory options, before they become a MemoryModel.
        struct MemoryOptions
        {
            bf::memory_size_t memory_size; //!< number of cells on the tape
            std::string cell_size;         //!< "8", "16" or "32"
            bool no_wrapping;              //!< true when the user asked for clamping at the edges
        };

        //! Name of \a cell_size as accepted on the command line.
        std::string cell_size_name(bf::CellSize cell_size)
        {
            switch (cell_size)
            {
            case bf::CellSize::EightBit:
                return "8";
            case bf::CellSize::SixteenBit:
                return "16";
            case bf::CellSize::ThirtyTwoBit:
                return "32";
            default:
                abort();
            }
        }

        //! Parses a command-line cell width; the caller validates it first.
        /**
         * \pre \a name is one of "8", "16" or "32", which CLI11 enforces
         */
        bf::CellSize cell_size_from_name(std::string name)
        {
            if (name == "8")
                return bf::CellSize::EightBit;
            else if (name == "16")
                return bf::CellSize::SixteenBit;
            else if (name == "32")
                return bf::CellSize::ThirtyTwoBit;
            else
                abort();
        }

        //! Comparison operators accepted on the command line.
        const std::vector<std::string> comparison_names{"=", ">", ">=", "<", "<="};

        //! Parses a command-line comparison; the caller validates it first.
        /**
         * \pre \a name is one of "=", ">", ">=", "<" or "<=", which CLI11
         *      enforces through \c comparison_names
         */
        bf::Comparison comparison_from_name(const std::string &name)
        {
            if (name == "=")
                return bf::Comparison::eq;
            if (name == ">")
                return bf::Comparison::gt;
            if (name == ">=")
                return bf::Comparison::ge;
            if (name == "<")
                return bf::Comparison::lt;
            if (name == "<=")
                return bf::Comparison::le;
            abort();
        }

        //! Wrap directions accepted on the command line.
        const std::vector<std::string> wrap_direction_names{"both", "overflow", "underflow"};

        //! Parses a command-line wrap direction; the caller validates it first.
        /**
         * \pre \a name is one of "both", "overflow" or "underflow", which CLI11
         *      enforces through \c wrap_direction_names
         */
        bf::WrapDirection wrap_direction_from_name(const std::string &name)
        {
            if (name == "both")
                return bf::WrapDirection::both;
            if (name == "overflow")
                return bf::WrapDirection::overflow;
            if (name == "underflow")
                return bf::WrapDirection::underflow;
            abort();
        }

        //! Defaults are taken from the model itself, never repeated here.
        MemoryOptions default_memory_options()
        {
            bf::MemoryModel default_model = bf::MemoryModel();
            return MemoryOptions{
                default_model.get_memory_size(),
                cell_size_name(default_model.get_cell_size()),
                !default_model.is_wrapping()};
        }

        //! Registers the three memory options on \a subcommand.
        /**
         * CLI11 enforces the valid range, so the callbacks can assume a
         * positive tape size and one of the three supported cell widths.
         */
        void add_memory_options(CLI::App *subcommand, MemoryOptions &options)
        {
            subcommand
                ->add_option("--memory-size", options.memory_size, "number of cells in the memory tape")
                ->check(CLI::PositiveNumber);
            subcommand
                ->add_option("--cell-size", options.cell_size, "size of a single cell in bits")
                ->check(CLI::IsMember(std::vector<std::string>{"8", "16", "32"}));
            subcommand->add_flag("--no-wrapping", options.no_wrapping, "stop at the tape edges instead of wrapping around");
        }

        //! Builds the MemoryModel described by \a options.
        bf::MemoryModel make_memory_model(const MemoryOptions &options)
        {
            bf::MemoryModel memory_model = bf::MemoryModel();
            memory_model.set_memory_size(options.memory_size);
            memory_model.set_cell_size(cell_size_from_name(options.cell_size));
            memory_model.set_wrapping(!options.no_wrapping);
            return memory_model;
        }

        //! Raw values of the standard-input options.
        struct IOOptions
        {
            unsigned int max_stdin_length = 0; //!< input budget; only meaningful when the option was given
            std::string input;                 //!< pinned input with its escapes still undecoded
            bf::io_char_t eof_char = 0;        //!< character delivered at end of input
            bool no_change_on_eof = false;     //!< leave the cell unchanged at end of input
        };

        //! Keeps track of which IO options the user actually supplied.
        /**
         * An option that was not given must not override the IOModel's own
         * default, which is why the options are recorded rather than compared
         * against a sentinel.
         */
        struct IOOptionRefs
        {
            CLI::Option *max_stdin_length = nullptr; //!< `--max-stdin-length`
            CLI::Option *input = nullptr;            //!< `--input`
            CLI::Option *eof_char = nullptr;         //!< `--eof-char`
            CLI::Option *no_change_on_eof = nullptr; //!< `--no-change-on-eof`
        };

        //! CLI11 validator for `--input`: the decoder's error message, or empty when it decodes.
        /**
         * Reuses \c decode_input_escapes() so the accepted syntax cannot drift
         * between the validation and the model that later consumes the value.
         */
        std::string validate_input(const std::string &value)
        {
            try
            {
                bf::decode_input_escapes(value);
            }
            catch (const std::invalid_argument &e)
            {
                return std::string(e.what());
            }
            return std::string();
        }

        //! Registers the three input options on \a subcommand and returns their handles.
        /**
         * Defaults are the IOModel's own, so an option that was not given does
         * not override them. `--input` pins the input to concrete bytes and
         * therefore excludes `--max-stdin-length`: the length is the input's.
         */
        IOOptionRefs add_io_options(CLI::App *subcommand, IOOptions &options)
        {
            IOOptionRefs refs;
            refs.max_stdin_length = subcommand->add_option(
                "--max-stdin-length", options.max_stdin_length, "maximum amount of characters read from standard in");
            refs.input = subcommand->add_option(
                                     "--input", options.input,
                                     "pin standard in to these bytes instead of exploring every byte "
                                     "(escapes: \\\\ \\n \\t \\r \\0 \\xNN)")
                             ->excludes(refs.max_stdin_length)
                             ->check(validate_input);
            refs.eof_char = subcommand->add_option(
                "--eof-char", options.eof_char, "character delivered when standard in is exhausted");
            refs.no_change_on_eof = subcommand->add_flag(
                "--no-change-on-eof", options.no_change_on_eof, "leave a cell unchanged when standard in is exhausted");
            return refs;
        }

        //! Builds the IOModel described by the supplied options only.
        bf::IOModel make_io_model(const IOOptions &options, const IOOptionRefs &refs)
        {
            bf::IOModel io_model = bf::IOModel();
            if (*refs.max_stdin_length)
                io_model.set_chars_until_eof(options.max_stdin_length);
            // Decoding scans the argument, so only do it when the user supplied one.
            if (*refs.input)
                io_model.set_input(bf::decode_input_escapes(options.input));
            if (*refs.eof_char)
                io_model.set_eof_char(options.eof_char);
            if (*refs.no_change_on_eof)
                io_model.set_no_change_on_eof(true);
            return io_model;
        }
    }

    int run_with_args(
        int argc, char **argv,
        ExecuteFun exfun,
        PrintFun pfun,
        DotFun dotfun,
        ReachFun reachfun,
        VisitFun visitfun,
        ValueFun valuefun,
        OverflowFun overflowfun)
    {
        CLI::App app;
        CLI::Option *version_flag = app.add_flag("--version,-v", "Print version");

        MemoryOptions memory_options = default_memory_options();
        IOOptions io_options;

        std::string filepath;
        CLI::App *execute = app.add_subcommand("execute", "execute a brainfuck program");
        execute->add_option("filepath", filepath, "brainfuck file to execute")->required();
        add_memory_options(execute, memory_options);
        IOOptionRefs execute_io_options = add_io_options(execute, io_options);

        CLI::App *print = app.add_subcommand("print", "print a brainfuck program");
        print->add_option("filepath", filepath, "brainfuck file to print")->required();
        CLI::Option *no_label_flag = print->add_flag("--no-labels", "don't included labels in output");

        CLI::App *dot = app.add_subcommand("dot", "print a dot graph of a brainfuck program's kripke structure");
        dot->add_option("filepath", filepath, "brainfuck file to analyze")->required();
        add_memory_options(dot, memory_options);
        // Folding is an analysis detail; this flag only exposes the graph with
        // every instruction as its own state, for debugging.
        bool no_compress = false;
        dot->add_flag("--no-compress", no_compress, "do not fold straight-line instruction runs into one state");

        std::string label;
        CLI::App *check = app.add_subcommand("check", "check a property of a brainfuck program");
        CLI::App *reach = check->add_subcommand("reach", "check whether a label is reached on every infinite run");
        reach->add_option("filepath", filepath, "brainfuck file to analyze")->required();
        reach->add_option("label", label, "label to use for reachability analysis")->required();
        add_memory_options(reach, memory_options);
        IOOptionRefs reach_io_options = add_io_options(reach, io_options);

        bf::mem_ptr_t cell = 0;
        bool infinitely_often = false;
        CLI::App *visit = check->add_subcommand("visit", "check whether a cell is visited on every infinite run");
        visit->add_option("filepath", filepath, "brainfuck file to analyze")->required();
        visit->add_option("cell", cell, "0-based index of the cell to check")->required();
        visit->add_flag(
            "--infinitely-often", infinitely_often,
            "require the cell to be visited infinitely often before halting");
        add_memory_options(visit, memory_options);
        IOOptionRefs visit_io_options = add_io_options(visit, io_options);

        bf::mem_ptr_t value_cell = 0;
        std::string comparison = "=";
        std::uint64_t target = 0;
        bool value_every_run = false;
        bool on_halt = false;
        CLI::App *value_cmd = check->add_subcommand("value", "check whether a cell reaches a value");
        value_cmd->add_option("filepath", filepath, "brainfuck file to analyze")->required();
        value_cmd->add_option("cell", value_cell, "0-based index of the cell to check")->required();
        value_cmd->add_option("op", comparison, "comparison applied to the cell content")
            ->required()
            ->check(CLI::IsMember(comparison_names));
        value_cmd->add_option("value", target, "value to compare the cell content with")->required();
        value_cmd->add_flag(
            "--every-run", value_every_run,
            "require every run to reach the value instead of asking for a witness");
        value_cmd->add_flag(
            "--on-halt", on_halt,
            "observe the cell only in the state where the program halts");
        add_memory_options(value_cmd, memory_options);
        IOOptionRefs value_io_options = add_io_options(value_cmd, io_options);

        bf::mem_ptr_t overflow_cell = 0;
        std::string wrap_direction = "both";
        CLI::App *overflow_cmd = check->add_subcommand(
            "overflow", "check whether a cell can overflow or underflow");
        overflow_cmd->add_option("filepath", filepath, "brainfuck file to analyze")->required();
        overflow_cmd->add_option("cell", overflow_cell, "0-based index of the cell to check")->required();
        overflow_cmd->add_option("--direction", wrap_direction, "which wrap to look for")
            ->check(CLI::IsMember(wrap_direction_names));
        add_memory_options(overflow_cmd, memory_options);
        IOOptionRefs overflow_io_options = add_io_options(overflow_cmd, io_options);

        app.require_subcommand(0, 1);
        CLI11_PARSE(app, argc, argv);

        if (*version_flag)
        {
            std::cout
                << "braincheck pre alpha"
                << std::endl
                << "Running with CLI11 " << CLI11_VERSION
                << std::endl;
            return 0;
        }
        else if (app.got_subcommand(execute))
        {
            exfun(filepath, make_memory_model(memory_options), make_io_model(io_options, execute_io_options));
        }
        else if (app.got_subcommand(print))
        {
            bool without_label = *no_label_flag ? true : false;
            pfun(filepath, without_label);
        }
        else if (app.got_subcommand(dot))
        {
            dotfun(filepath, make_memory_model(memory_options), !no_compress);
        }
        else if (reach->parsed())
        {
            reachfun(filepath, label, make_memory_model(memory_options), make_io_model(io_options, reach_io_options));
        }
        else if (visit->parsed())
        {
            visitfun(
                filepath, cell,
                infinitely_often ? bf::VisitKind::infinitely_often : bf::VisitKind::at_least_once,
                make_memory_model(memory_options),
                make_io_model(io_options, visit_io_options));
        }
        else if (value_cmd->parsed())
        {
            valuefun(
                filepath, value_cell, comparison_from_name(comparison), target,
                value_every_run ? bf::ValueKind::every_run : bf::ValueKind::any_run, on_halt,
                make_memory_model(memory_options),
                make_io_model(io_options, value_io_options));
        }
        else if (overflow_cmd->parsed())
        {
            overflowfun(
                filepath, overflow_cell, wrap_direction_from_name(wrap_direction),
                make_memory_model(memory_options),
                make_io_model(io_options, overflow_io_options));
        }
        else if (app.got_subcommand(check))
        {
            std::cout << check->help();
        }
        else
        {
            std::cout << app.help();
        }
        return 0;
    }
}
