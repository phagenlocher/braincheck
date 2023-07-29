#include <string>
#include <optional>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/emptiness.hh>
#include "analysis.hpp"
#include "program.hpp"
#include "kripke.hpp"

namespace brainfuck
{
    std::optional<spot::twa_run_ptr> check_reach(Program prog, std::string label)
    {
        auto d = spot::make_bdd_dict();
        auto f = spot::formula::F(spot::formula::ap(label));
        auto nf = spot::formula::Not(f);
        spot::twa_graph_ptr af = spot::translator(d).run(nf);
        auto k = std::make_shared<Kripke>(prog, d);
        if (auto run = k->intersecting_run(af))
        {
            return std::optional<spot::twa_run_ptr>{run};
        }
        else
        {
            return std::nullopt;
        }
    }
}