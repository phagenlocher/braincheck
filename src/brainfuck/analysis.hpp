#pragma once

#include <string>
#include <optional>
#include <spot/twa/twa.hh>
#include "program.hpp"

namespace brainfuck
{
    std::optional<spot::twa_run_ptr> check_reach(Program prog, std::string label);
}