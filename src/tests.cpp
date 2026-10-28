#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <map>
#include <minunit.h>
#include <brainfuck.hpp>

using namespace brainfuck;

//! Parses the hello-world program and ignores prose without opcodes.
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

//! A closing bracket without an opening one is a parse error.
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

//! An opening bracket left at the end is a parse error.
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

//! Inline labels are registered, and unknown labels are not.
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

//! A label behind the last instruction marks the end point.
MU_TEST(parsing_label_at_end)
{
    std::istringstream source("+-_last_");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    mu_check(prog.instr_count() == 2);
    mu_check(prog.has_label("last"));
    // The label marks the (empty) instruction pointer behind the program.
    auto label = prog.label_for_instr_ptr(2);
    mu_check(label.has_value());
    mu_check(label.value() == "last");
    mu_check(!prog.instr_for_pc(2).has_value());
}

//! Several labels on one instruction pointer are all kept.
MU_TEST(parsing_duplicate_labels)
{
    std::istringstream source("_first__second_+");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    // Two labels on the same instruction pointer are both kept.
    mu_check(prog.has_label("first"));
    mu_check(prog.has_label("second"));
    mu_check(prog.get_label_map().count(0) == static_cast<std::size_t>(2));
}

//! Source tokenisation, label registration and bracket balance.
MU_TEST_SUITE(parsing)
{
    MU_RUN_TEST(parsing_ok);
    MU_RUN_TEST(parsing_imbalanced1);
    MU_RUN_TEST(parsing_imbalanced2);
    MU_RUN_TEST(parsing_labels);
    MU_RUN_TEST(parsing_label_at_end);
    MU_RUN_TEST(parsing_duplicate_labels);
}

//! Cell values wrap at the configured cell width.
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

//! The pointer wraps or clamps at the tape edges as configured.
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

//! The input budget is consumed and EOF follows the configured policy.
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

//! A pinned input makes every read deterministic and ends per the EOF policy.
MU_TEST(io_model_concrete_input)
{
    IOModel io = IOModel();
    io.set_input(std::string("a\0b", 3));
    io.set_eof_char(42);

    // Pinning the input sets the budget to its length, so nothing is unbounded.
    mu_check(io.has_input());
    mu_check(io.get_chars_until_eof().has_value());
    mu_check(io.get_chars_until_eof().value() == 3);

    // A NUL byte is an ordinary input byte, not the end of the input.
    mu_check(io.read_possible_chars() == std::vector<io_char_t>{'a'});
    mu_check(io.read_possible_chars() == std::vector<io_char_t>{0});
    mu_check(io.read_possible_chars() == std::vector<io_char_t>{'b'});
    mu_check(io.get_chars_until_eof().value() == 0);
    mu_check(io.read_possible_chars() == std::vector<io_char_t>{42});

    // The pure variant reports the next byte without advancing the budget.
    IOModel pure = IOModel();
    pure.set_input("xyz");
    mu_check(pure.get_possible_chars(pure.get_chars_until_eof()) == std::vector<io_char_t>{'x'});
    mu_check(pure.get_chars_until_eof().value() == 3);
    mu_check(pure.get_possible_chars(2) == std::vector<io_char_t>{'y'});
    mu_check(pure.get_possible_chars(1) == std::vector<io_char_t>{'z'});
    mu_check(pure.get_possible_chars(0) == std::vector<io_char_t>{0});

    // An empty input is an exhausted input from the first read on.
    IOModel empty = IOModel(5);
    empty.set_input("");
    mu_check(empty.get_chars_until_eof().value() == 0);
    mu_check(empty.read_possible_chars() == std::vector<io_char_t>{0});
    empty.set_no_change_on_eof(true);
    mu_check(empty.read_possible_chars().empty());

    // The interpreter path reads the pinned bytes and never falls back to stdin.
    IOModel interpreter = IOModel();
    interpreter.set_input("hi");
    auto first = interpreter.read_next_char();
    mu_check(first.has_value());
    mu_check(first.value() == 'h');
    auto second = interpreter.read_next_char();
    mu_check(second.has_value());
    mu_check(second.value() == 'i');
    mu_check(!interpreter.read_next_char().has_value());
}

//! The `--input` escape syntax decodes to bytes and rejects malformed escapes.
MU_TEST(decode_input_escapes_syntax)
{
    mu_check(decode_input_escapes("plain") == "plain");
    mu_check(decode_input_escapes("") == "");
    mu_check(decode_input_escapes("a\\nb\\tc\\rd") == std::string("a\nb\tc\rd"));
    mu_check(decode_input_escapes("\\\\") == "\\");
    mu_check(decode_input_escapes("\\0") == std::string(1, '\0'));
    mu_check(decode_input_escapes("\\x41\\x62") == "Ab");
    mu_check(decode_input_escapes("\\x00") == std::string(1, '\0'));
    // Exactly two digits: `\x41bc` is `A` followed by "bc".
    mu_check(decode_input_escapes("\\x41bc") == "Abc");

    bool threw = false;
    try
    {
        decode_input_escapes("\\q");
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    mu_check(threw);

    threw = false;
    try
    {
        decode_input_escapes("\\x4");
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    mu_check(threw);

    threw = false;
    try
    {
        decode_input_escapes("\\");
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    mu_check(threw);
}

//! The pure transition helpers agree with the model's configuration.
MU_TEST(memory_model_transitions)
{
    MemoryModel wrapping = MemoryModel(CellSize::EightBit, 4, true);
    mu_check(wrapping.pointer_left(0) == 3);
    mu_check(wrapping.pointer_left(2) == 1);
    mu_check(wrapping.pointer_right(3) == 0);
    mu_check(wrapping.pointer_right(1) == 2);

    MemoryModel clamped = MemoryModel(CellSize::EightBit, 4, false);
    mu_check(clamped.pointer_left(0) == 0);
    mu_check(clamped.pointer_right(3) == 3);

    MemoryModel mm8bit = MemoryModel(CellSize::EightBit, 1, false);
    mu_check(mm8bit.next_value(255) == 0);
    mu_check(mm8bit.next_value(7) == 8);
    mu_check(mm8bit.prev_value(0) == 255);
    mu_check(mm8bit.prev_value(8) == 7);
    mu_check(mm8bit.normalize_value(256) == 0);
    mu_check(mm8bit.normalize_value(257) == 1);

    MemoryModel mm16bit = MemoryModel(CellSize::SixteenBit, 1, false);
    mu_check(mm16bit.next_value(65535) == 0);
    mu_check(mm16bit.prev_value(0) == 65535);
    mu_check(mm16bit.normalize_value(65536) == 0);

    MemoryModel mm32bit = MemoryModel(CellSize::ThirtyTwoBit, 1, false);
    mu_check(mm32bit.next_value(UINT32_MAX) == 0);
    mu_check(mm32bit.prev_value(0) == UINT32_MAX);
    mu_check(mm32bit.normalize_value(UINT32_MAX + 1ull) == 0);
}

//! Defaults are the documented ones and the setters take effect.
MU_TEST(memory_model_configuration)
{
    MemoryModel mm = MemoryModel();
    mu_check(mm.get_cell_size() == DEFAULT_CELL_SIZE);
    mu_check(mm.get_memory_size() == DEFAULT_MEMORY_SIZE);
    mu_check(mm.is_wrapping() == DEFAULT_WRAPPING);

    mm.set_cell_size(CellSize::SixteenBit);
    mm.set_memory_size(8);
    mm.set_wrapping(false);
    mu_check(mm.get_cell_size() == CellSize::SixteenBit);
    mu_check(mm.get_memory_size() == 8);
    mu_check(!mm.is_wrapping());
}

//! MemoryModel and IOModel configuration and transitions.
MU_TEST_SUITE(models)
{
    MU_RUN_TEST(memory_model_cell_size_wrapping);
    MU_RUN_TEST(memory_model_wrapping);
    MU_RUN_TEST(memory_model_transitions);
    MU_RUN_TEST(memory_model_configuration);
    MU_RUN_TEST(io_model_chars_until_eof);
    MU_RUN_TEST(io_model_concrete_input);
    MU_RUN_TEST(decode_input_escapes_syntax);
}

//! Setting or erasing a cell never changes earlier snapshots.
MU_TEST(memory_map_immutability)
{
    const MemoryMap empty;
    mu_check(empty.empty());
    mu_check(empty.size() == 0);
    mu_check(empty.get(3) == 0);
    mu_check(empty.to_std_map().empty());

    const MemoryMap one = empty.set(3, 7);
    mu_check(!one.empty());
    mu_check(one.size() == 1);
    mu_check(one.get(3) == 7);
    // Setting a cell must not touch the snapshot it was derived from.
    mu_check(empty.empty());
    mu_check(empty.get(3) == 0);

    const MemoryMap two = one.set(4, 8);
    mu_check(two.size() == 2);
    mu_check(two.get(3) == 7);
    mu_check(two.get(4) == 8);
    mu_check(one.size() == 1);
    mu_check(one.get(4) == 0);

    // Zero means absent, so setting a cell to zero removes it again.
    const MemoryMap cleared = two.set(3, 0);
    mu_check(cleared.size() == 1);
    mu_check(cleared.get(3) == 0);
    mu_check(cleared.get(4) == 8);
    // Removing a cell must leave the snapshot it came from intact, including
    // the subtree that hung off the removed node.
    mu_check(two.size() == 2);
    mu_check(two.get(3) == 7);
    mu_check(two.get(4) == 8);
    mu_check(one.set(3, 7).compare(one) == 0);
}

//! Random updates match std::map, keep old snapshots intact and hash order-independently.
MU_TEST(memory_map_matches_std_map)
{
    std::map<mem_ptr_t, cell_value_t> reference;
    MemoryMap map;
    unsigned long long state = 12345;

    for (int step = 0; step < 500; step++)
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        const mem_ptr_t cell = static_cast<mem_ptr_t>((state >> 33) % 1000);
        const cell_value_t value = static_cast<cell_value_t>((state >> 13) % 3);

        // Keep the previous snapshot so that every operation can be checked to
        // leave it untouched: snapshots are shared, not copied.
        const MemoryMap previous = map;
        const std::map<mem_ptr_t, cell_value_t> previous_reference = reference;

        if (value == 0)
            reference.erase(cell);
        else
            reference[cell] = value;
        map = map.set(cell, value);

        mu_check(map.get(cell) == value);
        mu_check(map.size() == reference.size());
        mu_check(map.to_std_map() == reference);
        mu_check(previous.size() == previous_reference.size());
        mu_check(previous.to_std_map() == previous_reference);

        // Equal contents hash and compare equal, whatever order built them.
        const MemoryMap rebuilt(reference);
        mu_check(rebuilt.hash() == map.hash());
        mu_check(rebuilt.compare(map) == 0);
        mu_check(rebuilt == map);
    }
}

//! compare() is a strict weak ordering consistent with operator==.
MU_TEST(memory_map_ordering)
{
    const MemoryMap a = MemoryMap().set(0, 1);
    const MemoryMap b = MemoryMap().set(0, 2);
    const MemoryMap c = MemoryMap().set(1, 1);

    mu_check(a.compare(a) == 0);
    mu_check(a.compare(b) != 0);
    // Ordering has to be antisymmetric for the search's state table.
    mu_check((a.compare(b) < 0) != (b.compare(a) < 0));
    mu_check(a.compare(c) != 0);
    mu_check(c.compare(a) != 0);
    mu_check(a == a);
    mu_check(a != b);
}

//! Persistent memory snapshots: immutability, ordering and hashing.
MU_TEST_SUITE(memory_maps)
{
    MU_RUN_TEST(memory_map_immutability);
    MU_RUN_TEST(memory_map_matches_std_map);
    MU_RUN_TEST(memory_map_ordering);
}

//! The hash covers pc, pointers, memory and input budget; zero cells are canonical.
MU_TEST(KState_hashing_basics)
{
    std::map<mem_ptr_t, cell_value_t> empty_memory{};
    std::map<mem_ptr_t, cell_value_t> memory1{std::make_pair(0, 1)};
    std::map<mem_ptr_t, cell_value_t> memory2{std::make_pair(0, 1), std::make_pair(1, 2)};
    std::map<mem_ptr_t, cell_value_t> memory3{std::make_pair(1, 2), std::make_pair(0, 1), std::make_pair(3, 0)};
    std::map<mem_ptr_t, cell_value_t> memory4{std::make_pair(0, 0), std::make_pair(1, 0)};
    std::map<mem_ptr_t, cell_value_t> memory5{std::make_pair(1, 1)};
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
    // Hash and compare have to agree: the memory contents are part of the
    // state identity, so different contents must hash differently.
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
    KState state_with_memory(0, 0, memory1);
    KState state_with_same_memory(0, 0, memory1);
    KState state_with_other_memory(0, 0, memory2);
    mu_check(state_with_memory.compare(state_with_same_memory) == 0);
    mu_check(state_with_memory.compare(state_with_other_memory) != 0);
    mu_check(
        KState(0, 0, empty_memory, std::make_optional(1)).hash() !=
        KState().hash());
    mu_check(
        KState(0, 0, empty_memory, std::make_optional(1)).hash() !=
        KState(0, 0, empty_memory, std::make_optional(2)).hash());
}

//! compare() covers memory contents and agrees with hash().
MU_TEST(KState_compare_covers_memory)
{
    const std::map<mem_ptr_t, cell_value_t> one{{0, 1}};
    const std::map<mem_ptr_t, cell_value_t> two{{0, 2}};

    const KState a(0, 0, one, std::make_optional<std::size_t>(3));
    const KState b(0, 0, two, std::make_optional<std::size_t>(3));
    const KState c(0, 0, one, std::make_optional<std::size_t>(3));

    // Same pointers and the same input budget still differ in their memory.
    mu_check(a.compare(b) != 0);
    mu_check((a.compare(b) < 0) != (b.compare(a) < 0));
    // Equal states compare equal and hash equal.
    mu_check(a.compare(c) == 0);
    mu_check(a.hash() == c.hash());
}

//! KState hash/compare agreement, which the search's hash table depends on.
MU_TEST_SUITE(hashing)
{
    MU_RUN_TEST(KState_hashing_basics);
    MU_RUN_TEST(KState_compare_covers_memory);
}

//! True when \a pc is the first instruction of a block.
static bool is_block_start(const Program &prog, instr_ptr_t pc)
{
    if (pc == 0 || pc >= prog.instr_count())
        return true;
    if (prog.get_label_map().count(pc) > 0)
        return true;

    const Instruction previous = prog.instr_for_pc(pc - 1).value();
    const Instruction current = prog.instr_for_pc(pc).value();
    const bool previous_value = previous == Instruction::inc || previous == Instruction::dec;
    const bool current_value = current == Instruction::inc || current == Instruction::dec;
    const bool previous_pointer = previous == Instruction::left || previous == Instruction::right;
    const bool current_pointer = current == Instruction::left || current == Instruction::right;
    return !((previous_value && current_value) || (previous_pointer && current_pointer));
}

//! Runs fold within their instruction class and stop before a label.
MU_TEST(blocks_split_at_labels_and_classes)
{
    std::istringstream source("+++-_l_---");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    mu_check(prog.instr_count() == 7);

    // + + + - fold into one block, the label forces the next - - - to start
    // a block of its own.
    const Block first = prog.block_at(0);
    mu_check(first.kind == BlockKind::values);
    mu_check(first.end == 4);
    mu_check(first.delta == 2);
    mu_check(prog.block_at(3).end == 4);

    const Block second = prog.block_at(4);
    mu_check(second.kind == BlockKind::values);
    mu_check(second.end == 7);
    mu_check(second.delta == -3);

    mu_check(prog.block_end(0) == 4);
    mu_check(prog.block_end(4) == 7);
    mu_check(prog.block_end(7) == 7);
}

//! A clamping pointer block is its composed bounds, not its net displacement.
MU_TEST(blocks_compose_pointer_runs_for_clamping_tapes)
{
    std::istringstream source(">>><");
    Program prog = Program::parse_from_istream(
        &source, MemoryModel(CellSize::EightBit, 3, false), IOModel());

    const Block block = prog.block_at(1);
    mu_check(block.kind == BlockKind::pointers);
    mu_check(block.end == 4);
    mu_check(block.delta == 2);
    // On a three-cell clamping tape the third '>' stays on cell 2 and the '<'
    // then reaches 1, so the composed function is constant 1, not ptr + 2.
    mu_check(block.low == 1);
    mu_check(block.high == 1);
}

//! Blocks tile every instruction and never hide a jump target.
MU_TEST(blocks_tile_the_program_and_keep_jump_targets)
{
    const std::string sources[] = {
        "+++-_l_---",
        "++>><<[->+<]>>[-]_end_.",
        ">><<><",
        "+_a_+_a_+++"};

    for (const std::string &text : sources)
    {
        std::istringstream source(text);
        Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());

        // Blocks tile the instructions: every pc belongs to exactly one block,
        // interior pcs are not starts and every block end is the next start.
        instr_ptr_t pc = 0;
        while (pc < prog.instr_count())
        {
            const Block block = prog.block_at(pc);
            mu_check(block.end > pc);
            mu_check(block.end <= prog.instr_count());
            for (instr_ptr_t interior = pc + 1; interior < block.end; interior++)
                mu_check(!is_block_start(prog, interior));
            if (block.end < prog.instr_count())
                mu_check(is_block_start(prog, block.end));
            pc = block.end;
        }

        // A jump target is the instruction after a bracket, which always starts
        // a block; folding must never hide a branch destination.
        for (instr_ptr_t bracket = 0; bracket < prog.instr_count(); bracket++)
        {
            const Instruction instruction = prog.instr_for_pc(bracket).value();
            if (instruction == Instruction::fwd || instruction == Instruction::bwd)
                mu_check(is_block_start(prog, prog.jump_target(bracket)));
        }
    }
}

//! Straight-line block construction and the invariants folding relies on.
MU_TEST_SUITE(blocks)
{
    MU_RUN_TEST(blocks_split_at_labels_and_classes);
    MU_RUN_TEST(blocks_compose_pointer_runs_for_clamping_tapes);
    MU_RUN_TEST(blocks_tile_the_program_and_keep_jump_targets);
}

//! Successors apply the configured wrapping, clamping and cell width.
MU_TEST(kripke_successors_follow_memory_model)
{
    std::istringstream wrapping_source("<");
    Program wrapping_prog = Program::parse_from_istream(
        &wrapping_source,
        MemoryModel(CellSize::EightBit, 4, true),
        IOModel());

    const Kripke wrapping_kripke(wrapping_prog);
    KIterator wrapping_it = wrapping_kripke.successors(wrapping_kripke.initial_state());
    mu_check(wrapping_it.first());
    mu_check(!wrapping_it.done());
    const KState wrapping_successor = wrapping_it.state();
    // Four cells and wrapping: left from 0 must reach the last cell.
    mu_check(wrapping_successor.get_mem_ptr() == 3);

    std::istringstream clamped_source("<");
    Program clamped_prog = Program::parse_from_istream(
        &clamped_source,
        MemoryModel(CellSize::EightBit, 4, false),
        IOModel());
    const Kripke clamped_kripke(clamped_prog);
    KIterator clamped_it = clamped_kripke.successors(clamped_kripke.initial_state());
    mu_check(clamped_it.first());
    const KState clamped_successor = clamped_it.state();
    mu_check(clamped_successor.get_mem_ptr() == 0);

    std::istringstream dec_source("-");
    Program dec_prog = Program::parse_from_istream(
        &dec_source,
        MemoryModel(CellSize::SixteenBit, 4, true),
        IOModel());
    const Kripke dec_kripke(dec_prog);
    KIterator dec_it = dec_kripke.successors(dec_kripke.initial_state());
    mu_check(dec_it.first());
    const KState dec_successor = dec_it.state();
    // Sixteen bit cells: 0 - 1 must wrap to 65535, not to 255.
    mu_check(dec_successor.get_memory().at(0) == 65535);
}

//! A read yields one successor per character, the last one included.
MU_TEST(kripke_explores_every_input)
{
    std::istringstream source(",");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog);
    KIterator it = kripke.successors(kripke.initial_state());

    cell_value_t max_seen = 0;
    size_t count = 0;
    for (it.first(); !it.done(); it.next())
    {
        const KState successor = it.state();
        const auto memory = successor.get_memory();
        const cell_value_t value = memory.count(0) > 0 ? memory.at(0) : 0;
        if (value > max_seen)
            max_seen = value;
        count++;
    }
    // One successor per possible input character, including the last one.
    mu_check(count == static_cast<size_t>(std::numeric_limits<io_char_t>::max()) + 1);
    mu_check(max_seen == std::numeric_limits<io_char_t>::max());
}

//! A label lives at its instruction pointer, and `+` before it is one state.
MU_TEST(kripke_label_lives_at_its_pointer)
{
    std::istringstream source("+_done_");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog);

    const KState init = kripke.initial_state();
    mu_check(init.get_instr_ptr() == 0);
    mu_check(!prog.has_label_at(init.get_instr_ptr(), "done"));

    KIterator it = kripke.successors(init);
    mu_check(it.first());
    const KState successor = it.state();
    // The label sits behind the last instruction and is a state of its own.
    mu_check(successor.get_instr_ptr() == 1);
    mu_check(prog.has_label_at(successor.get_instr_ptr(), "done"));
}

//! Only a read consumes the budget, and the successor reflects it.
MU_TEST(kripke_read_consumes_input_budget)
{
    std::istringstream source(",,");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel(1));
    const Kripke kripke(prog);

    const KState init = kripke.initial_state();
    mu_check(init.get_remaining_stdin_chars().has_value());
    mu_check(init.get_remaining_stdin_chars().value() == 1);

    KIterator it = kripke.successors(init);
    std::size_t count = 0;
    for (it.first(); !it.done(); it.next())
        count++;
    // Every possible character is a successor of the read.
    mu_check(count == static_cast<std::size_t>(std::numeric_limits<io_char_t>::max()) + 1);

    it.first();
    const KState after_read = it.state();
    mu_check(after_read.get_instr_ptr() == 1);
    // Only a read consumes input, so exactly one character is gone.
    mu_check(after_read.get_remaining_stdin_chars().has_value());
    mu_check(after_read.get_remaining_stdin_chars().value() == 0);
}

//! A pinned input gives a read exactly one successor instead of 256.
MU_TEST(kripke_concrete_input_is_deterministic)
{
    std::istringstream source(",,,");
    IOModel io_model = IOModel();
    io_model.set_input("ab");
    io_model.set_eof_char(7);
    Program prog = Program::parse_from_istream(&source, MemoryModel(), io_model);
    const Kripke kripke(prog);

    const KState init = kripke.initial_state();
    mu_check(init.get_remaining_stdin_chars().has_value());
    mu_check(init.get_remaining_stdin_chars().value() == 2);

    // A pinned read is deterministic: one successor, not the whole alphabet.
    KIterator first_read = kripke.successors(init);
    std::size_t count = 0;
    for (first_read.first(); !first_read.done(); first_read.next())
        count++;
    mu_check(count == 1);

    first_read.first();
    const KState after_first = first_read.state();
    mu_check(after_first.get_memory_map().get(0) == static_cast<cell_value_t>('a'));
    mu_check(after_first.get_remaining_stdin_chars().value() == 1);

    KIterator second_read = kripke.successors(after_first);
    mu_check(second_read.first());
    const KState after_second = second_read.state();
    mu_check(after_second.get_memory_map().get(0) == static_cast<cell_value_t>('b'));
    mu_check(after_second.get_remaining_stdin_chars().value() == 0);

    // Behind the last byte only the end-of-input policy decides the value.
    KIterator third_read = kripke.successors(after_second);
    mu_check(third_read.first());
    mu_check(!third_read.next());
    // next() moved past the only successor, so rewind before materialising it.
    third_read.first();
    const KState exhausted = third_read.state();
    mu_check(exhausted.get_memory_map().get(0) == 7);
}

//! The end point has exactly one successor, itself, with equal hash.
MU_TEST(kripke_end_state_stutters)
{
    std::istringstream source("+");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog);

    KIterator after_plus = kripke.successors(kripke.initial_state());
    mu_check(after_plus.first());
    const KState halted = after_plus.state();
    mu_check(prog.is_end(halted.get_instr_ptr()));

    // The end point has exactly one successor: itself.
    KIterator stutter = kripke.successors(halted);
    mu_check(stutter.first());
    mu_check(!stutter.done());
    const KState again = stutter.state();
    mu_check(again == halted);
    mu_check(again.hash() == halted.hash());
    stutter.next();
    mu_check(stutter.done());
}

//! A run of + and - becomes one transition with the net delta.
MU_TEST(kripke_folds_value_runs)
{
    std::istringstream source("+++-");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog);

    KIterator it = kripke.successors(kripke.initial_state());
    mu_check(it.first());
    const KState successor = it.state();
    // The whole run is one transition from pc 0 to pc 4.
    mu_check(successor.get_instr_ptr() == 4);
    mu_check(successor.get_memory().at(0) == 2);
    it.next();
    mu_check(it.done());
}

//! A folded value run reduces its delta modulo the cell width.
MU_TEST(kripke_folds_value_runs_modulo_cell_width)
{
    std::istringstream source(std::string(256, '+'));
    Program prog = Program::parse_from_istream(
        &source, MemoryModel(CellSize::EightBit, 30000, true), IOModel());
    const Kripke kripke(prog);

    KIterator it = kripke.successors(kripke.initial_state());
    mu_check(it.first());
    const KState successor = it.state();
    mu_check(successor.get_instr_ptr() == 256);
    // 256 increments cancel on an eight bit cell, and zero cells are absent.
    mu_check(successor.get_memory().count(0) == 0);
}

//! A folded pointer run uses net displacement when wrapping, bounds when clamping.
MU_TEST(kripke_folds_pointer_runs)
{
    std::istringstream wrapping_source(">>>>>><<<<");
    Program wrapping_prog = Program::parse_from_istream(
        &wrapping_source, MemoryModel(CellSize::EightBit, 4, true), IOModel());
    const Kripke wrapping_kripke(wrapping_prog);
    KIterator wrapping_it = wrapping_kripke.successors(wrapping_kripke.initial_state());
    mu_check(wrapping_it.first());
    const KState wrapping_successor = wrapping_it.state();
    // Six rights and four lefts: net two on a wrapping tape.
    mu_check(wrapping_successor.get_instr_ptr() == 10);
    mu_check(wrapping_successor.get_mem_ptr() == 2);

    std::istringstream clamped_source(">>><");
    Program clamped_prog = Program::parse_from_istream(
        &clamped_source, MemoryModel(CellSize::EightBit, 3, false), IOModel());
    const Kripke clamped_kripke(clamped_prog);
    KIterator clamped_it = clamped_kripke.successors(clamped_kripke.initial_state());
    mu_check(clamped_it.first());
    const KState clamped_successor = clamped_it.state();
    // The run saturates at the tape edge, so its net displacement is wrong.
    mu_check(clamped_successor.get_instr_ptr() == 4);
    mu_check(clamped_successor.get_mem_ptr() == 1);
}

//! Folding never crosses a label, so the labelled pointer stays a state.
MU_TEST(kripke_folding_keeps_labelled_block_boundaries)
{
    std::istringstream source("++_l_++");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog);

    const KState init = kripke.initial_state();
    mu_check(!prog.has_label_at(init.get_instr_ptr(), "l"));

    KIterator it = kripke.successors(init);
    mu_check(it.first());
    const KState labelled = it.state();
    // The label between the two runs is a state of its own.
    mu_check(labelled.get_instr_ptr() == 2);
    mu_check(prog.has_label_at(labelled.get_instr_ptr(), "l"));

    KIterator after = kripke.successors(labelled);
    mu_check(after.first());
    const KState end = after.state();
    mu_check(end.get_instr_ptr() == 4);
}

//! Folding preserves the interpreted run and only shortens the state count.
MU_TEST(kripke_folding_preserves_the_unique_run)
{
    std::istringstream source("+++>++<[->+<]");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());

    prog.run();
    const mem_ptr_t interpreted_pointer = prog.memory_model.get_pointer();
    const cell_value_t interpreted_zero = prog.memory_model.get_value(0);
    const cell_value_t interpreted_one = prog.memory_model.get_value(1);

    const Kripke folded(prog, Folding::straight_line);
    const Kripke plain(prog, Folding::none);

    // Follows the unique run to the end, counting transitions.
    const auto follow = [&prog](const Kripke &kripke, std::size_t &steps)
    {
        KState end = kripke.initial_state();
        steps = 0;
        while (!prog.is_end(end.get_instr_ptr()) && steps < 1000)
        {
            KIterator it = kripke.successors(end);
            if (!it.first())
                break;
            end = it.state();
            steps++;
        }
        return end;
    };

    std::size_t folded_steps = 0;
    std::size_t plain_steps = 0;
    const KState folded_end = follow(folded, folded_steps);
    const KState plain_end = follow(plain, plain_steps);

    mu_check(prog.is_end(folded_end.get_instr_ptr()));
    mu_check(folded_steps < plain_steps);
    mu_check(folded_end == plain_end);

    const auto cells = folded_end.get_memory();
    mu_check(folded_end.get_mem_ptr() == interpreted_pointer);
    mu_check((cells.count(0) > 0 ? cells.at(0) : 0) == interpreted_zero);
    mu_check((cells.count(1) > 0 ? cells.at(1) : 0) == interpreted_one);
}

//! A label name at several pointers counts at each of them.
/**
 * A repeated name must count at every pointer carrying it; the search looks
 * the name up per state, so the invariant is that neither occurrence is lost.
 */
MU_TEST(kripke_repeated_label_name_is_at_every_pointer)
{
    std::istringstream source("_l_>+_l_");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());

    mu_check(prog.has_label_at(0, "l"));
    mu_check(!prog.has_label_at(1, "l"));
    mu_check(prog.has_label_at(2, "l"));
}

//! State generation, label lookup, input handling and folding.
MU_TEST_SUITE(kripke)
{
    MU_RUN_TEST(kripke_successors_follow_memory_model);
    MU_RUN_TEST(kripke_explores_every_input);
    MU_RUN_TEST(kripke_label_lives_at_its_pointer);
    MU_RUN_TEST(kripke_read_consumes_input_budget);
    MU_RUN_TEST(kripke_concrete_input_is_deterministic);
    MU_RUN_TEST(kripke_end_state_stutters);
    MU_RUN_TEST(kripke_folds_value_runs);
    MU_RUN_TEST(kripke_folds_value_runs_modulo_cell_width);
    MU_RUN_TEST(kripke_folds_pointer_runs);
    MU_RUN_TEST(kripke_folding_keeps_labelled_block_boundaries);
    MU_RUN_TEST(kripke_folding_preserves_the_unique_run);
    MU_RUN_TEST(kripke_repeated_label_name_is_at_every_pointer);
}

//! A label that every run reaches is reported as reached.
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

//! A label that some run can skip is reported as unreachable.
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

//! A finite input budget can make an otherwise skippable label unavoidable.
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

//! A label at the end point is reached by every halting run.
MU_TEST(check_reach_label_at_end_of_halting_program)
{
    // A label behind the last instruction marks the halted state, so a program
    // that always halts always "reaches" it.
    std::string halting = "+++_done_";
    std::istringstream source(halting);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program prog = Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(prog.has_label("done"));
        auto m_run = check_reach(prog, "done");
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

//! A label the halting run skipped yields a counterexample, not a vacuous success.
MU_TEST(check_reach_halting_program_can_miss_label)
{
    // The loop is skipped, the program halts, and the label inside the loop is
    // never visited. Because the halted state stutters, this is a
    // counterexample and not a vacuously "always reached" label.
    std::string halting = "[_x_+]+";
    std::istringstream source(halting);
    MemoryModel memory_model = MemoryModel();
    IOModel io_model = IOModel();
    try
    {
        Program prog = Program::parse_from_istream(&source, memory_model, io_model);
        mu_check(prog.has_label("x"));
        auto m_run = check_reach(prog, "x");
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

//! Folding changes the number of states, never the reachability answer.
MU_TEST(check_reach_is_independent_of_folding)
{
    const std::pair<std::string, std::string> cases[] = {
        {"+++++[->+++++[->+++++<]<]>>[-]_end_.", "end"},
        {"+[,]_end_.", "end"},
        {"[_x_]+", "x"},
        {"+++_done_", "done"},
        {"+++>++<[->+<]_done_", "done"},
    };

    for (const auto &entry : cases)
    {
        std::istringstream folded_source(entry.first);
        Program folded = Program::parse_from_istream(
            &folded_source, MemoryModel(), IOModel());
        std::istringstream plain_source(entry.first);
        Program plain = Program::parse_from_istream(
            &plain_source, MemoryModel(), IOModel());

        mu_check(folded.has_label(entry.second));
        const auto folded_run = check_reach(folded, entry.second, true);
        const auto plain_run = check_reach(plain, entry.second, false);
        mu_check(folded_run.has_value() == plain_run.has_value());
    }
}

//! Pinning the input can turn "some run misses the label" into "no run does".
MU_TEST(check_reach_concrete_input_changes_the_answer)
{
    // `,` writes the byte, `[` skips the loop on zero. The label sits on the
    // loop body, so it is visited exactly when the byte is not zero.
    std::string source_text = ",[_l_-]";
    MemoryModel memory_model = MemoryModel();

    // Symbolic input still explores a skipping run, so the label is skippable.
    std::istringstream symbolic_source(source_text);
    Program symbolic = Program::parse_from_istream(&symbolic_source, memory_model, IOModel());
    mu_check(!symbolic.io_model.has_input());
    mu_check(check_reach(symbolic, "l").has_value());

    // Byte 0 skips the loop, so the label is not always reached.
    IOModel zero_input = IOModel();
    zero_input.set_input(std::string(1, '\0'));
    std::istringstream zero_source(source_text);
    Program zero = Program::parse_from_istream(&zero_source, memory_model, zero_input);
    mu_check(zero.io_model.has_input());
    mu_check(check_reach(zero, "l").has_value());

    // Byte 1 enters the loop and visits the label on the only run there is.
    IOModel one_input = IOModel();
    one_input.set_input(std::string(1, '\x01'));
    std::istringstream one_source(source_text);
    Program one = Program::parse_from_istream(&one_source, memory_model, one_input);
    mu_check(!check_reach(one, "l").has_value());
}

//! check_reach answers on reachable, unreachable and halting programs.
MU_TEST_SUITE(analysis)
{
    MU_RUN_TEST(check_reach_ok);
    MU_RUN_TEST(check_reach_nonreachable);
    MU_RUN_TEST(check_reach_limited_input);
    MU_RUN_TEST(check_reach_concrete_input_changes_the_answer);
    MU_RUN_TEST(check_reach_label_at_end_of_halting_program);
    MU_RUN_TEST(check_reach_halting_program_can_miss_label);
    MU_RUN_TEST(check_reach_is_independent_of_folding);
}

//! A cell on the only run is visited, a cell the pointer never reaches is not.
MU_TEST(check_visit_at_least_once)
{
    std::istringstream source("+>");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    // The initial state counts for cell 0, and the halting state counts for
    // cell 1: the final `>` really did move the pointer there.
    mu_check(!check_visit(prog, 0, VisitKind::at_least_once).has_value());
    mu_check(!check_visit(prog, 1, VisitKind::at_least_once).has_value());
    mu_check(check_visit(prog, 2, VisitKind::at_least_once).has_value());
}

//! The empty program visits its initial cell for F, but not infinitely often.
MU_TEST(check_visit_empty_program)
{
    std::istringstream source("");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    mu_check(!check_visit(prog, 0, VisitKind::at_least_once).has_value());
    // The start point is also the end point, so the halting state is excluded.
    mu_check(check_visit(prog, 0, VisitKind::infinitely_often).has_value());
}

//! An infinite loop that stays on a cell visits it infinitely often.
MU_TEST(check_visit_infinitely_often_success)
{
    std::istringstream source("+[]");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    mu_check(!check_visit(prog, 0, VisitKind::infinitely_often).has_value());
    mu_check(check_visit(prog, 1, VisitKind::infinitely_often).has_value());

    // The pointer moves right and left on every iteration. Pointer runs must
    // stay unfolded, otherwise the fold of `><` would hide cell 1 entirely.
    std::istringstream moving_source("+[><]");
    Program moving = Program::parse_from_istream(
        &moving_source, MemoryModel(), IOModel());
    mu_check(!check_visit(moving, 0, VisitKind::infinitely_often).has_value());
    mu_check(!check_visit(moving, 1, VisitKind::infinitely_often).has_value());
    mu_check(check_visit(moving, 2, VisitKind::infinitely_often).has_value());
}

//! An infinite run can leave a cell behind, which fails infinite visitation.
MU_TEST(check_visit_infinitely_often_leaves_cell)
{
    std::istringstream source("+>+[]");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    // The empty loop runs forever on cell 1; cell 0 is only in the prefix.
    mu_check(check_visit(prog, 0, VisitKind::infinitely_often).has_value());
    mu_check(!check_visit(prog, 1, VisitKind::infinitely_often).has_value());
}

//! A halting program never satisfies infinite visitation before halting.
MU_TEST(check_visit_halting_program_fails_infinitely_often)
{
    std::istringstream plus_source("+");
    Program plus = Program::parse_from_istream(&plus_source, MemoryModel(), IOModel());
    // Without excluding the halting state this would succeed through the stutter.
    mu_check(check_visit(plus, 0, VisitKind::infinitely_often).has_value());

    std::istringstream loop_source("++[>+<-]");
    Program loop = Program::parse_from_istream(&loop_source, MemoryModel(), IOModel());
    mu_check(check_visit(loop, 0, VisitKind::infinitely_often).has_value());
    mu_check(check_visit(loop, 1, VisitKind::infinitely_often).has_value());
}

//! Cell visitation follows the input model, like label reachability.
MU_TEST(check_visit_respects_input)
{
    const std::string text = ",[>]";

    // Symbolic input explores the skipping run, so cell 1 is not guaranteed.
    std::istringstream symbolic_source(text);
    Program symbolic = Program::parse_from_istream(&symbolic_source, MemoryModel(), IOModel());
    mu_check(check_visit(symbolic, 1, VisitKind::at_least_once).has_value());

    // Byte 1 enters the loop and reaches cell 1.
    IOModel one_input = IOModel();
    one_input.set_input(std::string(1, '\x01'));
    std::istringstream one_source(text);
    Program one = Program::parse_from_istream(&one_source, MemoryModel(), one_input);
    mu_check(!check_visit(one, 1, VisitKind::at_least_once).has_value());

    // Byte 0 skips the loop, so cell 1 is never reached.
    IOModel zero_input = IOModel();
    zero_input.set_input(std::string(1, '\0'));
    std::istringstream zero_source(text);
    Program zero = Program::parse_from_istream(&zero_source, MemoryModel(), zero_input);
    mu_check(check_visit(zero, 1, VisitKind::at_least_once).has_value());
}

//! Value folding changes neither visitation answer.
MU_TEST(check_visit_folding_independent)
{
    const std::pair<std::string, mem_ptr_t> cases[] = {
        {"+>", 0}, {"+>", 1}, {"+>", 2},
        {"+[]", 0}, {"+>+[]", 0}, {"+>+[]", 1},
        {">>", 2}, {">>", 3}};

    for (const auto &entry : cases)
    {
        std::istringstream folded_source(entry.first);
        Program folded = Program::parse_from_istream(
            &folded_source, MemoryModel(), IOModel());
        std::istringstream plain_source(entry.first);
        Program plain = Program::parse_from_istream(
            &plain_source, MemoryModel(), IOModel());

        for (const VisitKind kind : {VisitKind::at_least_once, VisitKind::infinitely_often})
        {
            const auto folded_run = check_visit(folded, entry.second, kind, true);
            const auto plain_run = check_visit(plain, entry.second, kind, false);
            mu_check(folded_run.has_value() == plain_run.has_value());
        }
    }
}

//! A repeated label name must not turn cell-visit answers vacuous.
MU_TEST(check_visit_duplicate_label_name)
{
    std::istringstream source("_l_+_l_");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    // Nothing moves to cell 1, so a counterexample must be found. The
    // repeated name must not hide the states that carry it.
    mu_check(check_visit(prog, 1, VisitKind::at_least_once).has_value());
    mu_check(!check_visit(prog, 0, VisitKind::at_least_once).has_value());
}

//! A visit observation follows the memory pointer.
MU_TEST(kripke_visit_observation_follows_mem_ptr)
{
    std::istringstream source("+");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    Observation observation;
    observation.visited_cell = 0;
    const Kripke kripke(prog, Folding::values, observation);

    const std::map<mem_ptr_t, cell_value_t> empty;
    const KState on_cell(0, 0, empty, std::nullopt);
    const KState off_cell(0, 1, empty, std::nullopt);
    // Same pc and same non-end point, but the pointer is on a different cell.
    mu_check(kripke.observes(on_cell));
    mu_check(!kripke.observes(off_cell));
}

//! Folding::values keeps every pointer move as its own state.
MU_TEST(kripke_values_folding_keeps_pointer_states)
{
    std::istringstream source(">>");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());

    const Kripke values(prog, Folding::values);
    const KState init = values.initial_state();
    KIterator first_it = values.successors(init);
    mu_check(first_it.first());
    const KState first = first_it.state();
    mu_check(first.get_instr_ptr() == 1);
    mu_check(first.get_mem_ptr() == 1);

    KIterator second_it = values.successors(first);
    mu_check(second_it.first());
    const KState second = second_it.state();
    mu_check(second.get_instr_ptr() == 2);
    mu_check(second.get_mem_ptr() == 2);

    // Folding::straight_line still collapses both moves into one transition.
    const Kripke straight(prog, Folding::straight_line);
    KIterator straight_it = straight.successors(straight.initial_state());
    mu_check(straight_it.first());
    const KState straight_end = straight_it.state();
    mu_check(straight_end.get_instr_ptr() == 2);
    mu_check(straight_end.get_mem_ptr() == 2);
}

//! Folding::values still folds + and - runs, because they keep the pointer.
MU_TEST(kripke_values_folding_folds_value_runs)
{
    std::istringstream source("+++>");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog, Folding::values);

    KIterator it = kripke.successors(kripke.initial_state());
    mu_check(it.first());
    const KState successor = it.state();
    mu_check(successor.get_instr_ptr() == 3);
    mu_check(successor.get_mem_ptr() == 0);
    mu_check(successor.get_memory_map().get(0) == 3);
}
//! Cell visitation checks and the folding/condition support they need.
MU_TEST_SUITE(cell_visits)
{
    MU_RUN_TEST(check_visit_at_least_once);
    MU_RUN_TEST(check_visit_empty_program);
    MU_RUN_TEST(check_visit_infinitely_often_success);
    MU_RUN_TEST(check_visit_infinitely_often_leaves_cell);
    MU_RUN_TEST(check_visit_halting_program_fails_infinitely_often);
    MU_RUN_TEST(check_visit_respects_input);
    MU_RUN_TEST(check_visit_folding_independent);
    MU_RUN_TEST(check_visit_duplicate_label_name);
    MU_RUN_TEST(kripke_visit_observation_follows_mem_ptr);
    MU_RUN_TEST(kripke_values_folding_keeps_pointer_states);
    MU_RUN_TEST(kripke_values_folding_folds_value_runs);
}

namespace
{
    //! Parses \a text under the given models, or nothing when it does not parse.
    std::optional<Program> parse_or_nothing(
        const std::string &text,
        MemoryModel memory_model = MemoryModel(),
        IOModel io_model = IOModel())
    {
        std::istringstream source(text);
        try
        {
            return Program::parse_from_istream(&source, memory_model, io_model);
        }
        catch (const std::exception &)
        {
            return std::nullopt;
        }
    }

    //! True when some state of \a run satisfies \a test.
    /**
     * A value check returns either a counterexample or a witness run; this
     * scans both parts for a state that the test accepts, so a test can assert
     * that the printed run really supports the answer.
     */
    bool run_satisfies(const Program &prog, const Run &run, const CellTest &test)
    {
        const auto hit = [&prog, &test](const KState &state)
        {
            if (test.on_halt && !prog.is_end(state.get_instr_ptr()))
                return false;
            return cell_test_holds(test, state.get_memory_map().get(test.cell));
        };
        for (const KState &state : run.prefix)
            if (hit(state))
                return true;
        for (const KState &state : run.cycle)
            if (hit(state))
                return true;
        return false;
    }

    //! True when some state of \a run wraps \a test's cell on its next step.
    /**
     * A wrap check returns a counterexample run; this scans both parts for the
     * state before the wrapping step, so a test can assert that the printed run
     * really supports the answer.
     */
    bool run_wraps(const Program &prog, const Run &run, const WrapTest &test)
    {
        const auto hit = [&prog, &test](const KState &state)
        {
            return wrap_test_holds(prog, state, test);
        };
        for (const KState &state : run.prefix)
            if (hit(state))
                return true;
        for (const KState &state : run.cycle)
            if (hit(state))
                return true;
        return false;
    }
}

//! Cell 0 holds zero before the first instruction, so value 0 is reached.
MU_TEST(check_value_initial_state_counts)
{
    const auto prog = parse_or_nothing("");
    mu_check(prog.has_value());
    mu_check(check_value(prog.value(), CellTest{0, Comparison::eq, 0, false},
                         ValueKind::every_run).holds);
    // Nothing but the empty initial memory exists, so 1 is never reached.
    mu_check(!check_value(prog.value(), CellTest{0, Comparison::eq, 1, false},
                          ValueKind::every_run).holds);
}

//! A value passed through inside a `+` run is observable.
MU_TEST(check_value_inside_value_run)
{
    const auto prog = parse_or_nothing("+++++");
    mu_check(prog.has_value());
    // Folding the run for a value check would hide the intermediate 3.
    mu_check(check_value(prog.value(), CellTest{0, Comparison::eq, 3, false},
                         ValueKind::every_run).holds);
    mu_check(check_value(prog.value(), CellTest{0, Comparison::eq, 3, false},
                         ValueKind::any_run).holds);
}

//! The comparisons are applied to the cell content, not to a source value.
MU_TEST(check_value_comparisons)
{
    const auto prog = parse_or_nothing("+++");
    mu_check(prog.has_value());
    const Program &program = prog.value();
    // The only run passes 1, 2 and 3 in cell 0.
    mu_check(check_value(program, CellTest{0, Comparison::eq, 3, false}, ValueKind::every_run).holds);
    mu_check(check_value(program, CellTest{0, Comparison::ge, 3, false}, ValueKind::every_run).holds);
    mu_check(check_value(program, CellTest{0, Comparison::lt, 3, false}, ValueKind::every_run).holds);
    mu_check(check_value(program, CellTest{0, Comparison::le, 3, false}, ValueKind::every_run).holds);
    mu_check(check_value(program, CellTest{0, Comparison::gt, 2, false}, ValueKind::every_run).holds);
    // Nothing in the run exceeds 3.
    mu_check(!check_value(program, CellTest{0, Comparison::gt, 3, false}, ValueKind::every_run).holds);
    mu_check(!check_value(program, CellTest{0, Comparison::eq, 4, false}, ValueKind::any_run).holds);
}

//! A value outside the cell range answers instead of being rejected.
MU_TEST(check_value_out_of_range_value)
{
    const auto prog = parse_or_nothing("+++");
    mu_check(prog.has_value());
    const Program &program = prog.value();
    // An 8-bit cell is always <= 300 and can never be >= 300.
    mu_check(check_value(program, CellTest{0, Comparison::le, 300, false}, ValueKind::every_run).holds);
    mu_check(!check_value(program, CellTest{0, Comparison::ge, 300, false}, ValueKind::every_run).holds);
    mu_check(!check_value(program, CellTest{0, Comparison::eq, 300, false}, ValueKind::any_run).holds);
}

//! Cell values wrap, so a decrement from zero reaches the largest value.
MU_TEST(check_value_wraps_at_cell_width)
{
    const auto narrow = parse_or_nothing("-");
    mu_check(narrow.has_value());
    mu_check(check_value(narrow.value(), CellTest{0, Comparison::eq, 255, false},
                         ValueKind::every_run).holds);

    const auto wide = parse_or_nothing("-", MemoryModel(CellSize::SixteenBit, 30000, true));
    mu_check(wide.has_value());
    mu_check(!check_value(wide.value(), CellTest{0, Comparison::eq, 255, false},
                          ValueKind::every_run).holds);
    mu_check(check_value(wide.value(), CellTest{0, Comparison::eq, 65535, false},
                         ValueKind::every_run).holds);
}

//! The any-run answer carries a witness that really satisfies the test.
MU_TEST(check_value_any_run_finds_the_run)
{
    const auto prog = parse_or_nothing("+++>+++");
    mu_check(prog.has_value());
    const CellTest test{1, Comparison::eq, 3, false};

    const ValueAnswer found = check_value(prog.value(), test, ValueKind::any_run);
    mu_check(found.holds);
    mu_check(found.run.has_value());
    mu_check(run_satisfies(prog.value(), found.run.value(), test));

    // A value the program never writes has no witness at all.
    const ValueAnswer missing =
        check_value(prog.value(), CellTest{1, Comparison::eq, 4, false}, ValueKind::any_run);
    mu_check(!missing.holds);
    mu_check(!missing.run.has_value());
}

//! Only a pinned input byte makes the value unavoidable.
/**
 * The symbolic case also guards that the observed atom may look at the memory
 * contents: the read's successors differ only in the byte they store, and each
 * of them must be evaluated on its own contents.
 */
MU_TEST(check_value_respects_input)
{
    const CellTest test{0, Comparison::eq, 'A', false};

    const auto symbolic = parse_or_nothing(",");
    mu_check(symbolic.has_value());
    // Every byte is possible, so some run misses the value, another hits it.
    mu_check(!check_value(symbolic.value(), test, ValueKind::every_run).holds);
    mu_check(check_value(symbolic.value(), test, ValueKind::any_run).holds);

    IOModel pinned = IOModel();
    pinned.set_input("A");
    const auto exact = parse_or_nothing(",", MemoryModel(), pinned);
    mu_check(exact.has_value());
    mu_check(check_value(exact.value(), test, ValueKind::every_run).holds);
}

//! A repeated label name must not turn value answers vacuous.
/**
 * `echo.bf` names both of its read states `read`. Every occurrence of a name
 * counts, so the analysis must still find the state that holds the byte.
 */
MU_TEST(check_value_duplicate_label_name)
{
    const CellTest test{0, Comparison::eq, 'A', false};

    IOModel pinned = IOModel();
    pinned.set_input("A");
    std::istringstream exact_source("_read_,[._read_,]");
    Program exact = Program::parse_from_istream(&exact_source, MemoryModel(), pinned);
    mu_check(check_value(exact, test, ValueKind::every_run).holds);
    const ValueAnswer any = check_value(exact, test, ValueKind::any_run);
    mu_check(any.holds);
    mu_check(any.run.has_value());
    mu_check(run_satisfies(exact, any.run.value(), test));

    // With one byte of symbolic input, another byte is a counterexample.
    std::istringstream symbolic_source("_read_,[._read_,]");
    Program symbolic = Program::parse_from_istream(
        &symbolic_source, MemoryModel(), IOModel(1));
    mu_check(!check_value(symbolic, test, ValueKind::every_run).holds);
    mu_check(check_value(symbolic, test, ValueKind::any_run).holds);
}

//! A postcondition observes the end point only, so intermediate values vanish.
MU_TEST(check_value_on_halt)
{
    const auto prog = parse_or_nothing("+++++");
    mu_check(prog.has_value());
    const Program &program = prog.value();

    // The run passes 3 on its way to 5, but only 5 is the halting value.
    mu_check(check_value(program, CellTest{0, Comparison::eq, 3, false}, ValueKind::every_run).holds);
    mu_check(!check_value(program, CellTest{0, Comparison::eq, 3, true}, ValueKind::every_run).holds);
    mu_check(check_value(program, CellTest{0, Comparison::eq, 5, true}, ValueKind::every_run).holds);

    // A program that never halts has no end point to observe.
    const auto forever = parse_or_nothing("+[]");
    mu_check(forever.has_value());
    mu_check(!check_value(forever.value(), CellTest{0, Comparison::eq, 1, true},
                          ValueKind::every_run).holds);
    mu_check(!check_value(forever.value(), CellTest{0, Comparison::eq, 1, true},
                          ValueKind::any_run).holds);
}

//! A halting witness really is reported at the end point.
MU_TEST(check_value_on_halt_witness_ends_at_halt)
{
    const auto prog = parse_or_nothing("+++");
    mu_check(prog.has_value());
    const CellTest test{0, Comparison::eq, 3, true};

    const ValueAnswer answer = check_value(prog.value(), test, ValueKind::any_run);
    mu_check(answer.holds);
    mu_check(answer.run.has_value());
    mu_check(run_satisfies(prog.value(), answer.run.value(), test));
}

//! A value observation is a function of the content, not of the pointer.
MU_TEST(kripke_value_observation_follows_memory)
{
    const auto prog = parse_or_nothing("+");
    mu_check(prog.has_value());
    Observation observation;
    observation.cell_test = CellTest{0, Comparison::eq, 1, false};
    const Kripke kripke(prog.value(), Folding::pointers, observation);

    const std::map<mem_ptr_t, cell_value_t> empty;
    const std::map<mem_ptr_t, cell_value_t> one{{0, 1}};
    const KState unsatisfied(0, 0, empty, std::nullopt);
    const KState satisfied(0, 0, one, std::nullopt);

    // Same pc and same pointer, different content: the observation differs.
    mu_check(!kripke.observes(unsatisfied));
    mu_check(kripke.observes(satisfied));
}

//! Folding::pointers folds pointer runs and keeps every value state.
MU_TEST(kripke_pointers_folding_keeps_value_states)
{
    const auto prog = parse_or_nothing("++>>");
    mu_check(prog.has_value());
    const Kripke folded(prog.value(), Folding::pointers);

    KIterator first_it = folded.successors(folded.initial_state());
    mu_check(first_it.first());
    const KState first = first_it.state();
    mu_check(first.get_instr_ptr() == 1);
    mu_check(first.get_memory_map().get(0) == 1);

    KIterator second_it = folded.successors(first);
    mu_check(second_it.first());
    const KState second = second_it.state();
    mu_check(second.get_instr_ptr() == 2);
    mu_check(second.get_memory_map().get(0) == 2);

    // The `>>` run is one transition, two cells of displacement.
    KIterator third_it = folded.successors(second);
    mu_check(third_it.first());
    const KState third = third_it.state();
    mu_check(third.get_instr_ptr() == 4);
    mu_check(third.get_mem_ptr() == 2);
}
//! Cell value checks and the folding/condition support they need.
MU_TEST_SUITE(cell_values)
{
    MU_RUN_TEST(check_value_initial_state_counts);
    MU_RUN_TEST(check_value_inside_value_run);
    MU_RUN_TEST(check_value_comparisons);
    MU_RUN_TEST(check_value_out_of_range_value);
    MU_RUN_TEST(check_value_wraps_at_cell_width);
    MU_RUN_TEST(check_value_any_run_finds_the_run);
    MU_RUN_TEST(check_value_respects_input);
    MU_RUN_TEST(check_value_duplicate_label_name);
    MU_RUN_TEST(check_value_on_halt);
    MU_RUN_TEST(check_value_on_halt_witness_ends_at_halt);
    MU_RUN_TEST(kripke_value_observation_follows_memory);
    MU_RUN_TEST(kripke_pointers_folding_keeps_value_states);
}

//! The increment that passes the cell maximum is an overflow counterexample.
MU_TEST(check_overflow_increment_at_max)
{
    const WrapTest overflow{0, WrapDirection::overflow};

    // 255 increments reach the maximum, the 256th wraps to zero.
    const auto wrapping = parse_or_nothing(std::string(256, '+'));
    mu_check(wrapping.has_value());
    const auto counterexample = check_overflow(wrapping.value(), overflow);
    mu_check(counterexample.has_value());
    mu_check(run_wraps(wrapping.value(), counterexample.value(), overflow));

    // 255 increments stop at the maximum: the cell never wraps.
    const auto stopping = parse_or_nothing(std::string(255, '+'));
    mu_check(stopping.has_value());
    mu_check(!check_overflow(stopping.value(), overflow).has_value());
}

//! A decrement from zero underflows, and only the underflow direction sees it.
MU_TEST(check_overflow_underflow)
{
    const auto prog = parse_or_nothing("-");
    mu_check(prog.has_value());
    const WrapTest underflow{0, WrapDirection::underflow};
    const auto counterexample = check_overflow(prog.value(), underflow);
    mu_check(counterexample.has_value());
    mu_check(run_wraps(prog.value(), counterexample.value(), underflow));

    // A `-` is not a `+`, so the same program never overflows.
    mu_check(!check_overflow(prog.value(), WrapTest{0, WrapDirection::overflow}).has_value());

    // `both` accepts the underflow as well.
    mu_check(check_overflow(prog.value(), WrapTest{0, WrapDirection::both}).has_value());
}

//! An ordinary run never wraps, so the statement holds and nothing is printed.
MU_TEST(check_overflow_without_wrap)
{
    const auto prog = parse_or_nothing("+++");
    mu_check(prog.has_value());
    mu_check(!check_overflow(prog.value(), WrapTest{0, WrapDirection::both}).has_value());
}

//! The stuttering end point has no instruction, so it never wraps.
MU_TEST(check_overflow_empty_program)
{
    const auto prog = parse_or_nothing("");
    mu_check(prog.has_value());
    mu_check(!check_overflow(prog.value(), WrapTest{0, WrapDirection::both}).has_value());
}

//! The cell width decides when the maximum is reached.
MU_TEST(check_overflow_follows_cell_width)
{
    const std::string program(256, '+');
    const WrapTest overflow{0, WrapDirection::overflow};

    const auto narrow = parse_or_nothing(program);
    mu_check(narrow.has_value());
    mu_check(check_overflow(narrow.value(), overflow).has_value());

    const auto wide = parse_or_nothing(
        program, MemoryModel(CellSize::SixteenBit, 30000, true));
    mu_check(wide.has_value());
    mu_check(!check_overflow(wide.value(), overflow).has_value());
}

//! Only the observed cell wraps, not the one the pointer merely passed.
MU_TEST(check_overflow_is_per_cell)
{
    // Cell 0 overflows, cell 1 underflows.
    const auto prog = parse_or_nothing("+[+]>-[-]-");
    mu_check(prog.has_value());
    const Program &program = prog.value();

    mu_check(check_overflow(program, WrapTest{0, WrapDirection::overflow}).has_value());
    mu_check(!check_overflow(program, WrapTest{0, WrapDirection::underflow}).has_value());
    mu_check(check_overflow(program, WrapTest{1, WrapDirection::underflow}).has_value());
    mu_check(!check_overflow(program, WrapTest{1, WrapDirection::overflow}).has_value());
    // `both` finds the wrap a cell really has.
    mu_check(check_overflow(program, WrapTest{0, WrapDirection::both}).has_value());
    mu_check(check_overflow(program, WrapTest{1, WrapDirection::both}).has_value());
}

//! A read that delivers the maximum makes the following increment wrap.
MU_TEST(check_overflow_respects_input)
{
    const WrapTest overflow{0, WrapDirection::overflow};

    // Some byte is 255, so some run wraps.
    const auto symbolic = parse_or_nothing(",+");
    mu_check(symbolic.has_value());
    mu_check(check_overflow(symbolic.value(), overflow).has_value());

    // A pinned byte below the maximum never wraps ...
    IOModel plain = IOModel();
    plain.set_input("A");
    const auto safe = parse_or_nothing(",+", MemoryModel(), plain);
    mu_check(safe.has_value());
    mu_check(!check_overflow(safe.value(), overflow).has_value());

    // ... while 255 itself does.
    IOModel maximum_input = IOModel();
    maximum_input.set_input(std::string(1, '\xff'));
    const auto pinned = parse_or_nothing(",+", MemoryModel(), maximum_input);
    mu_check(pinned.has_value());
    mu_check(check_overflow(pinned.value(), overflow).has_value());
}

//! Pointer runs are folded, but the value step that wraps stays a state.
MU_TEST(check_overflow_sees_through_pointer_folding)
{
    const MemoryModel tape(CellSize::EightBit, 4, true);
    const auto prog = parse_or_nothing(">>>" + std::string(256, '+'), tape);
    mu_check(prog.has_value());
    const WrapTest overflow{3, WrapDirection::overflow};
    const auto counterexample = check_overflow(prog.value(), overflow);
    mu_check(counterexample.has_value());
    mu_check(run_wraps(prog.value(), counterexample.value(), overflow));
}

//! The wrap observation is true in the state before the wrapping instruction.
MU_TEST(kripke_wrap_observation_follows_state)
{
    const auto prog = parse_or_nothing("+-");
    mu_check(prog.has_value());
    Observation observation;
    observation.wrap_test = WrapTest{0, WrapDirection::overflow};
    const Kripke kripke(prog.value(), Folding::pointers, observation);

    const std::map<mem_ptr_t, cell_value_t> empty;
    const std::map<mem_ptr_t, cell_value_t> maximum{{0, 255}};
    const KState at_max(0, 0, maximum, std::nullopt);     // `+` on 255: wraps
    const KState at_zero(0, 0, empty, std::nullopt);      // `+` on 0: does not
    const KState dec_at_max(1, 0, maximum, std::nullopt); // `-` on 255: no overflow
    const KState moved(0, 1, empty, std::nullopt);        // `+` on another cell

    mu_check(kripke.observes(at_max));
    mu_check(!kripke.observes(at_zero));
    mu_check(!kripke.observes(dec_at_max));
    mu_check(!kripke.observes(moved));
}

//! Overflow and underflow checks and the folding/condition support they need.
MU_TEST_SUITE(cell_overflows)
{
    MU_RUN_TEST(check_overflow_increment_at_max);
    MU_RUN_TEST(check_overflow_underflow);
    MU_RUN_TEST(check_overflow_without_wrap);
    MU_RUN_TEST(check_overflow_empty_program);
    MU_RUN_TEST(check_overflow_follows_cell_width);
    MU_RUN_TEST(check_overflow_is_per_cell);
    MU_RUN_TEST(check_overflow_respects_input);
    MU_RUN_TEST(check_overflow_sees_through_pointer_folding);
    MU_RUN_TEST(kripke_wrap_observation_follows_state);
}

//! The existential search returns a shortest witness path.
MU_TEST(search_eventually_is_shortest)
{
    std::istringstream source("+++++");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog, Folding::pointers);

    const auto run = search_eventually(
        kripke,
        [](const KState &state) { return state.get_memory_map().get(0) == 2; });
    mu_check(run.has_value());
    // 0, 1, 2: the shortest path to the first state that holds 2.
    mu_check(run.value().prefix.size() == 3);
    mu_check(run.value().cycle.empty());
    mu_check(run.value().prefix.back().get_memory_map().get(0) == 2);
}

//! A counterexample lasso really closes: its last state leads back to the first.
MU_TEST(search_always_closes_its_cycle)
{
    std::istringstream source("+[><]");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog, Folding::none);

    const auto run = search_always(kripke, [](const KState &) { return true; });
    mu_check(run.has_value());
    // The loop body `><` is three states: `]` jumps back to the `>`.
    mu_check(run.value().cycle.size() == 3);

    KIterator successors = kripke.successors(run.value().cycle.back());
    bool closes = false;
    for (successors.first(); !successors.done(); successors.next())
        if (successors.state() == run.value().cycle.front())
            closes = true;
    mu_check(closes);
}

//! A `F(G(P))` counterexample may reach its cycle through states outside P.
MU_TEST(search_eventually_always_prefix_is_unrestricted)
{
    std::istringstream source("+>+[]");
    Program prog = Program::parse_from_istream(&source, MemoryModel(), IOModel());
    const Kripke kripke(prog, Folding::values);

    const auto run = search_eventually_always(
        kripke,
        [](const KState &state) { return state.get_mem_ptr() == 1; });
    mu_check(run.has_value());
    mu_check(!run.value().prefix.empty());
    // The prefix starts on cell 0, outside the predicate ...
    mu_check(run.value().prefix.front().get_mem_ptr() == 0);
    // ... and only the cycle has to stay on cell 1.
    mu_check(!run.value().cycle.empty());
    for (const KState &state : run.value().cycle)
        mu_check(state.get_mem_ptr() == 1);
}

//! The three searches that decide the checks and the runs they return.
MU_TEST_SUITE(search)
{
    MU_RUN_TEST(search_eventually_is_shortest);
    MU_RUN_TEST(search_always_closes_its_cycle);
    MU_RUN_TEST(search_eventually_always_prefix_is_unrestricted);
}

//! Runs every suite and returns the minunit exit code.
int main()
{
    MU_RUN_SUITE(parsing);
    MU_RUN_SUITE(models);
    MU_RUN_SUITE(memory_maps);
    MU_RUN_SUITE(hashing);
    MU_RUN_SUITE(blocks);
    MU_RUN_SUITE(kripke);
    MU_RUN_SUITE(analysis);
    MU_RUN_SUITE(cell_visits);
    MU_RUN_SUITE(cell_values);
    MU_RUN_SUITE(cell_overflows);
    MU_RUN_SUITE(search);
    MU_REPORT();
    return MU_EXIT_CODE;
}
