#include <string>
#include <iostream>
#include <argparse.hpp>
#include <brainfuck.hpp>
#include <spot/twaalgos/dot.hh>
#include <spot/twaalgos/emptiness.hh>

#define RESET "\x1b[0m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define RED_BOLD "\x1b[31;1m"
#define GREEN_BOLD "\x1b[32;1m"
#define YELLOW_BOLD "\x1b[33;1m"
#define BLUE_BOLD "\x1b[34;1m"
#define MAGENTA_BOLD "\x1b[35;1m"
#define CYAN_BOLD "\x1b[36;1m"
#define WHITE_BOLD "\x1b[37;1m"

namespace ap = argparse;
namespace bf = brainfuck;

auto parse_bf_program =
    [](
        std::string filename,
        bf::MemoryModel memory_model = bf::MemoryModel(),
        bf::IOModel io_model = bf::IOModel())
{
    try
    {
        return bf::Program::parse_from_file(filename.c_str(), memory_model, io_model);
    }
    catch (bf::ParseException &pe)
    {
        std::cerr << RED_BOLD;
        std::cerr << "Parse error: " << pe.what() << std::endl;
        std::cerr << "Parsing of " << filename << " failed... Aborting!" << std::endl;
        std::cerr << RESET;
        exit(1);
    }
};

ap::ExecuteFun exfun = [](std::string filename)
{
    auto prog = parse_bf_program(filename);
    prog.run();
};

ap::PrintFun pfun = [](std::string filename, bool without_label)
{
    auto prog = parse_bf_program(filename);
    prog.print(without_label);
};

ap::DotFun dotfun = [](std::string filename)
{
    bf::Program prog = parse_bf_program(filename);
    auto k = std::make_shared<bf::Kripke>(prog, spot::make_bdd_dict());
    spot::print_dot(std::cout, k, "N");
};

ap::CheckReachFun crfun = [](std::string filename, std::string label, bf::IOModel io_model)
{
    bf::Program prog = parse_bf_program(filename, bf::MemoryModel(), io_model);

    if (!prog.has_label(label))
    {
        std::cerr << RED_BOLD;
        std::cerr
            << "Label \""
            << label
            << "\" does not exist in the specified program."
            << std::endl;
        std::cerr << RESET;
        exit(0);
    }

    auto m_run = bf::check_reach(prog, label);
    if (m_run.has_value())
    {
        auto run = m_run.value();
        std::cerr << RED_BOLD;
        std::cout
            << "There exists a run for which the label \""
            << label
            << "\" will not be reached:"
            << std::endl;
        bf::instr_ptr_t last_ptr = 0;
        std::cout << BLUE_BOLD;
        for (auto step : run->prefix)
        {
            auto ss = static_cast<const bf::KState *>(step.s);
            auto instr = prog.instr_for_pc(ss->get_instr_ptr());
            std::cout << bf::instr_char(instr.value());
        }
        std::cout << RED_BOLD;
        for (auto step : run->cycle)
        {
            auto ss = static_cast<const bf::KState *>(step.s);
            auto instr = prog.instr_for_pc(ss->get_instr_ptr());
            std::cout << bf::instr_char(instr.value());
            last_ptr = ss->get_instr_ptr();
        }
        std::cout << BLUE_BOLD;
        for (bf::instr_ptr_t i = last_ptr + 1; true; i++)
        {
            auto m_label = prog.label_for_instr_ptr(i);
            if (m_label.has_value())
            {
                if (m_label.value().compare(label) == 0)
                {
                    std::cout << YELLOW_BOLD
                              << bf::LABEL_SEPARATOR
                              << label
                              << bf::LABEL_SEPARATOR
                              << BLUE_BOLD;
                }
            }
            auto m_instr = prog.instr_for_pc(i);
            if (!m_instr.has_value())
                break;
            else
                std::cout << bf::instr_char(m_instr.value());
        }
    }
    else
    {
        std::cout << GREEN_BOLD;
        std::cout
            << "Label \""
            << label
            << "\" will always be reached.";
    }
    std::cout << RESET << std::endl;
};

int main(int argc, char **argv)
{
    return ap::run_with_args(
        argc, argv,
        exfun,
        pfun,
        dotfun,
        crfun);
}
