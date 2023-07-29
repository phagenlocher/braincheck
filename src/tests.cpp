#include <minunit.h>
#include <brainfuck.hpp>

using namespace brainfuck;

MU_TEST(parsing_ok)
{
    std::string hello_world =
        "Here is some text that should be ignored"
        ">+++[<++++++++++++++++++++++++>-]<.>+++[<+++++++++>-]<++.>+++[<++>-]<"
        "+..+++.>+++[<-------------------------->-]<-.>+++[<++++++++++++++++++"
        ">-]<+.>+++[<++++++++>-]<.+++.>+++[<-->-]<.>+++[<-->-]<--.[-]+++++++++"
        "+";
    std::istringstream source(hello_world);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(true);
    }
    catch (ParseException &pe)
    {
        mu_fail(pe.what());
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST(parsing_imbalanced1)
{
    std::string imbalanced = "+[+]+]+";
    std::istringstream source(imbalanced);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program::parse_from_istream(&source, memory_model, io_model);
        mu_fail("Parsing did NOT fail even though source had imbalanced parantheses.");
    }
    catch (ParseException &pe)
    {
        mu_check(true);
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST(parsing_imbalanced2)
{
    std::string imbalanced = "+][+";
    std::istringstream source(imbalanced);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program::parse_from_istream(&source, memory_model, io_model);
        mu_fail("Parsing did NOT fail even though source had imbalanced parantheses.");
    }
    catch (ParseException &pe)
    {
        mu_check(true);
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST(parsing_labels)
{
    std::string labeled = "+++_label1_-[-]-_label2_+++";
    std::istringstream source(labeled);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program prog = Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(prog.has_label("label1"));
        mu_check(prog.has_label("label2"));
        mu_check(!prog.has_label("label3"));
    }
    catch (ParseException &pe)
    {
        mu_fail(pe.what());
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST_SUITE(parsing)
{
    MU_RUN_TEST(parsing_ok);
    MU_RUN_TEST(parsing_imbalanced1);
    MU_RUN_TEST(parsing_imbalanced2);
    MU_RUN_TEST(parsing_labels);
}

MU_TEST(memory_model_cell_size_wrapping)
{
    MemoryModel mm8bit = MemoryModel(CellSize::EightBit, 1, false);
    mm8bit.set_current_value(255); // 2^8 - 1
    mu_check(mm8bit.get_current_value() == 255);
    mm8bit.set_current_value(256);
    mu_check(mm8bit.get_current_value() == 0);
    mm8bit.decrement();
    mu_check(mm8bit.get_current_value() == 255);

    MemoryModel mm16bit = MemoryModel(CellSize::SixteenBit, 1, false);
    mm16bit.set_current_value(65535); // 2^16 - 1
    mu_check(mm16bit.get_current_value() == 65535);
    mm16bit.set_current_value(65536);
    mu_check(mm16bit.get_current_value() == 0);
    mm16bit.decrement();
    mu_check(mm16bit.get_current_value() == 65535);

    MemoryModel mm32bit = MemoryModel(CellSize::ThirtyTwoBit, 1, false);
    mm32bit.set_current_value(4294967295); // 2^32 - 1
    mu_check(mm32bit.get_current_value() == 4294967295);
    mm32bit.increment();
    mu_check(mm32bit.get_current_value() == 0);
    mm32bit.decrement();
    mu_check(mm32bit.get_current_value() == 4294967295);
}

MU_TEST(memory_model_wrapping)
{
    MemoryModel non_wrapping_mm = MemoryModel(CellSize::EightBit, 4, false);
    mu_check(non_wrapping_mm.get_pointer() == 0);
    non_wrapping_mm.left();
    mu_check(non_wrapping_mm.get_pointer() == 0);
    non_wrapping_mm.right();
    non_wrapping_mm.right();
    non_wrapping_mm.right();
    non_wrapping_mm.right();
    mu_check(non_wrapping_mm.get_pointer() == 3);

    MemoryModel wrapping_mm = MemoryModel(CellSize::EightBit, 4, true);
    mu_check(wrapping_mm.get_pointer() == 0);
    wrapping_mm.left();
    mu_check(wrapping_mm.get_pointer() == 3);
    wrapping_mm.right();
    mu_check(wrapping_mm.get_pointer() == 0);
    wrapping_mm.right();
    wrapping_mm.right();
    wrapping_mm.right();
    wrapping_mm.right();
    mu_check(wrapping_mm.get_pointer() == 0);
}

MU_TEST(io_model_chars_until_eof)
{
    IOModel io = IOModel(1);
    auto chars = io.read_possible_chars();
    mu_check(chars.size() == 256);
    auto m_chars_until_eof = io.get_chars_until_eof();
    mu_check(m_chars_until_eof.has_value());
    mu_check(m_chars_until_eof.value() == 0);
    auto eof_chars = io.read_possible_chars();
    mu_check(eof_chars.size() == 1);
    mu_check(eof_chars[0] == 0);
    io.set_no_change_on_eof(true);
    auto no_chars = io.read_possible_chars();
    mu_check(no_chars.size() == 0);
}

MU_TEST_SUITE(models)
{
    MU_RUN_TEST(memory_model_cell_size_wrapping);
    MU_RUN_TEST(memory_model_wrapping);
    MU_RUN_TEST(io_model_chars_until_eof);
}

MU_TEST(KState_hashing_basics)
{
    std::map<mem_ptr_t, uint8_t> empty_memory{};
    std::map<mem_ptr_t, uint8_t> memory1{std::make_pair(0, 1)};
    std::map<mem_ptr_t, uint8_t> memory2{std::make_pair(0, 1), std::make_pair(1, 2)};
    std::map<mem_ptr_t, uint8_t> memory3{std::make_pair(1, 2), std::make_pair(0, 1), std::make_pair(3, 0)};
    std::map<mem_ptr_t, uint8_t> memory4{std::make_pair(0, 0), std::make_pair(1, 0)};
    std::map<mem_ptr_t, uint8_t> memory5{std::make_pair(1, 1)};
    mu_check(
        KState(0, 0, empty_memory).hash() ==
        KState().hash());
    mu_check(
        KState(0, 0, empty_memory, std::nullopt).hash() ==
        KState().hash());
    mu_check(
        KState(0, 0, empty_memory).hash() ==
        KState(0, 0, empty_memory).hash());
    mu_check(
        KState(1, 0, empty_memory).hash() !=
        KState(0, 0, empty_memory).hash());
    mu_check(
        KState(0, 1, empty_memory).hash() !=
        KState(0, 0, empty_memory).hash());
    mu_check(
        KState(1, 0, empty_memory).hash() !=
        KState(0, 1, empty_memory).hash());
    mu_check(
        KState(0, 0, memory1).hash() ==
        KState(0, 0, memory1).hash());
    mu_check(
        KState(0, 0, empty_memory).hash() !=
        KState(0, 0, memory1).hash());
    mu_check(
        KState(0, 0, memory1).hash() !=
        KState(0, 0, memory2).hash());
    mu_check(
        KState(0, 0, memory2).hash() ==
        KState(0, 0, memory3).hash());
    mu_check(
        KState(0, 0, empty_memory).hash() ==
        KState(0, 0, memory4).hash());
    mu_check(
        KState(0, 0, memory1).hash() !=
        KState(0, 0, memory5).hash());
    mu_check(
        KState(0, 0, empty_memory, std::make_optional(1)).hash() !=
        KState().hash());
    mu_check(
        KState(0, 0, empty_memory, std::make_optional(1)).hash() !=
        KState(0, 0, empty_memory, std::make_optional(2)).hash());
}

MU_TEST_SUITE(hashing)
{
    MU_RUN_TEST(KState_hashing_basics);
}

MU_TEST(check_reach_ok)
{
    std::string reachable = "+++++[->+++++[->+++++<]<]>>[-]_end_.";
    std::istringstream source(reachable);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program prog = Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(prog.has_label("end"));
        auto m_run = check_reach(prog, "end");
        mu_check(!m_run.has_value());
    }
    catch (ParseException &pe)
    {
        mu_fail(pe.what());
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST(check_reach_nonreachable)
{
    std::string nonreachable = "+[,]_end_.";
    std::istringstream source(nonreachable);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program prog = Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(prog.has_label("end"));
        auto m_run = check_reach(prog, "end");
        mu_check(m_run.has_value());
    }
    catch (ParseException &pe)
    {
        mu_fail(pe.what());
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST(check_reach_limited_input)
{
    std::string nonreachable = "+[,]_end_.";
    std::istringstream source(nonreachable);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel(5);
    try
    {
        Program prog = Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(prog.has_label("end"));
        auto m_run = check_reach(prog, "end");
        mu_check(!m_run.has_value());
    }
    catch (ParseException &pe)
    {
        mu_fail(pe.what());
    }
    catch (const std::exception &e)
    {
        mu_fail("Parsing failed with unknown exception!");
    }
}

MU_TEST_SUITE(analysis)
{
    MU_RUN_TEST(check_reach_ok);
    MU_RUN_TEST(check_reach_nonreachable);
    MU_RUN_TEST(check_reach_limited_input);
}

int main()
{
    MU_RUN_SUITE(parsing);
    MU_RUN_SUITE(models);
    MU_RUN_SUITE(hashing);
    MU_RUN_SUITE(analysis);
    MU_REPORT();
    return MU_EXIT_CODE;
}
