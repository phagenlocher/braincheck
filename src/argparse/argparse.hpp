#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <brainfuck.hpp>

namespace bf = brainfuck;

//! Command-line front end: parses arguments and dispatches to the callbacks.
/**
 * The namespace only parses and forwards. The actual work lives in the
 * callbacks supplied by `main.cpp`, which keeps argument handling and analysis
 * logic separated (see `AGENTS.md`).
 */
namespace argparse
{
    //! Callback for `execute`: run the program at \a filename.
    typedef void (*ExecuteFun)(std::string filename, bf::MemoryModel memory_model, bf::IOModel io_model);
    //! Callback for `print`: write the program at \a filename to stdout.
    typedef void (*PrintFun)(std::string filename, bool without_label);
    //! Callback for `dot`: print the Kripke structure of \a filename as a graph.
    typedef void (*DotFun)(std::string filename, bf::MemoryModel memory_model, bool compress);
    //! Callback for `check reach`: test whether \a label is always reached in \a filename.
    typedef void (*ReachFun)(std::string filename, std::string label, bf::MemoryModel memory_model, bf::IOModel io_model);
    //! Callback for `check visit`: test the requested visitation of \a cell in \a filename.
    typedef void (*VisitFun)(std::string filename, bf::mem_ptr_t cell, bf::VisitKind kind, bf::MemoryModel memory_model, bf::IOModel io_model);
    //! Callback for `check value`: test whether \a cell satisfies \a op \a value.
    /**
     * \a kind selects between "some run" (the default) and "every run";
     * \a on_halt restricts the observation to the end point.
     */
    typedef void (*ValueFun)(std::string filename, bf::mem_ptr_t cell, bf::Comparison op,
                             std::uint64_t value, bf::ValueKind kind, bool on_halt,
                             bf::MemoryModel memory_model, bf::IOModel io_model);
    //! Callback for `check overflow`: test whether \a cell can over- or underflow.
    /**
     * \a direction selects overflow, underflow or both. The check is always
     * existential: a witness is printed when some run wraps the cell.
     */
    typedef void (*OverflowFun)(std::string filename, bf::mem_ptr_t cell,
                                bf::WrapDirection direction,
                                bf::MemoryModel memory_model, bf::IOModel io_model);

    //! Parses \a argv and runs the subcommand's callback.
    /**
     * Memory and IO options start from the models' own defaults, so an option
     * the user did not supply does not override them. With no subcommand the
     * top-level help is printed; with a bare `check` the group's help is
     * printed.
     *
     * \param argc argument count as passed to `main`
     * \param argv argument vector as passed to `main`
     * \param exfun callback for `execute`
     * \param pfun callback for `print`
     * \param dotfun callback for `dot`
     * \param reachfun callback for `check reach`
     * \param visitfun callback for `check visit`
     * \param valuefun callback for `check value`
     * \param overflowfun callback for `check overflow`
     * \return 0 on success or after printing help; a CLI11 parse error returns
     *         CLI11's own exit code instead
     */
    int run_with_args(
        int argc,
        char **argv,
        ExecuteFun exfun,
        PrintFun pfun,
        DotFun dotfun,
        ReachFun reachfun,
        VisitFun visitfun,
        ValueFun valuefun,
        OverflowFun overflowfun);
}
