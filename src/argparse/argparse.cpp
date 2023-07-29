#include <iostream>
#include <CLI/CLI.hpp>
#include <brainfuck.hpp>
#include "argparse.hpp"
#include <spot/misc/version.hh>

namespace bf = brainfuck;

namespace argparse
{
    int run_with_args(
        int argc, char **argv,
        ExecuteFun exfun,
        PrintFun pfun,
        DotFun dotfun,
        CheckReachFun crfun)
    {
        CLI::App app;
        CLI::Option *version_flag = app.add_flag("--version,-v", "Print version");

        std::string filepath;
        CLI::App *execute = app.add_subcommand("execute", "execute a brainfuck program");
        execute->add_option("filepath", filepath, "brainfuck file to execute")->required();

        CLI::App *print = app.add_subcommand("print", "print a brainfuck program");
        print->add_option("filepath", filepath, "brainfuck file to print")->required();
        CLI::Option *no_label_flag = print->add_flag("--no-labels", "don't included labels in output");

        CLI::App *dot = app.add_subcommand("dot", "print a dot graph of a brainfuck program's kripke structure");
        dot->add_option("filepath", filepath, "brainfuck file to analyze")->required();

        std::string label;
        unsigned int max_stdin_length;
        uint8_t eof_char;
        CLI::App *checkreach = app.add_subcommand("check_reach", "check if a certain label can be reached");
        checkreach->add_option("filepath", filepath, "brainfuck file to analyze")->required();
        checkreach->add_option("label", label, "label to use for reachability analysis")->required();
        CLI::Option *max_stdin_len_opt = checkreach->add_option("--max-stdin-length", max_stdin_length, "maximum amount of character read on standard in");
        CLI::Option *eof_char_opt = checkreach->add_option("--eof-char", eof_char, "character to be used when EOF is signaled");
        CLI::Option *no_change_on_eof_flag = checkreach->add_flag("--no-change-on-eof", "don't change a cell's value when EOF is received");

        app.require_subcommand(0, 1);
        CLI11_PARSE(app, argc, argv);

        bf::IOModel io_model = bf::IOModel();
        if (*max_stdin_len_opt)
        {
            io_model.set_chars_until_eof(max_stdin_length);
        }
        if (*eof_char_opt)
        {
            io_model.set_eof_char(eof_char);
        }
        if (*no_change_on_eof_flag)
        {
            io_model.set_no_change_on_eof(true);
        }

        if (*version_flag)
        {
            std::cout
                << "braincheck pre alpha"
                << std::endl
                << "Running with Spot " << spot::version()
                << " and CLI11 " << CLI11_VERSION
                << std::endl;
            return 0;
        }
        else if (app.got_subcommand(execute))
        {
            exfun(filepath);
        }
        else if (app.got_subcommand(print))
        {
            bool without_label = *no_label_flag ? true : false;
            pfun(filepath, without_label);
        }
        else if (app.got_subcommand(dot))
        {
            dotfun(filepath);
        }
        else if (app.got_subcommand(checkreach))
        {

            crfun(filepath, label, io_model);
        }
        else
        {
            std::cout << app.help();
        }
        return 0;
    }
}