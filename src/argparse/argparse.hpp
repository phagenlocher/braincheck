#pragma once

#include <string>
#include <optional>
#include <brainfuck.hpp>

namespace bf = brainfuck;

namespace argparse
{
    typedef void (*ExecuteFun)(std::string filename);
    typedef void (*PrintFun)(std::string filename, bool without_label);
    typedef void (*DotFun)(std::string filename);
    typedef void (*CheckReachFun)(std::string filename, std::string label, bf::IOModel io_model);

    int run_with_args(
        int argc,
        char **argv,
        ExecuteFun exfun,
        PrintFun pfun,
        DotFun dotfun,
        CheckReachFun crfun);
}