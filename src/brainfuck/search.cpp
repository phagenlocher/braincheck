#include <algorithm>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include "kripke.hpp"
#include "search.hpp"

namespace brainfuck
{
    namespace
    {
        //! "No index" marker for the parent arrays.
        constexpr std::size_t no_index = static_cast<std::size_t>(-1);

        //! Colour of a state during a depth-first search.
        enum Colour : unsigned char
        {
            white = 0, //!< not visited yet
            gray = 1,  //!< on the current DFS path
            black = 2  //!< fully explored
        };

        //! The explored part of the state graph, in discovery order.
        /**
         * Only `search_eventually_always()` needs the whole graph, because its
         * cycle may start anywhere; the other two routines stop at the first
         * witness or close their cycle during the search.
         */
        struct Graph
        {
            //! State of every node.
            std::vector<KState> states;
            //! Breadth-first parent of every node; \c no_index at the root.
            std::vector<std::size_t> parent;
            //! Indices of the successors of every node.
            std::vector<std::vector<std::size_t>> successors;
        };

        //! Explores the states reachable through states that \a allowed admits.
        /**
         * Breadth first, so \c Graph::parent is a shortest-path tree and the
         * prefix of a counterexample found from it is minimal. A state that
         * \a allowed rejects is neither stored nor expanded, which turns the
         * exploration into the atom subgraph; passing an always-true predicate
         * explores the whole reachable graph.
         *
         * \param kripke the state graph to explore
         * \param allowed predicate deciding which states may be stored and expanded
         * \return the explored graph; empty when the initial state is rejected
         */
        Graph explore(const Kripke &kripke, const StatePredicate &allowed)
        {
            Graph graph;
            std::unordered_map<KState, std::size_t, KStateHash, KStateEqual> index;

            const KState start = kripke.initial_state();
            if (!allowed(start))
                return graph;

            index.emplace(start, 0);
            graph.states.push_back(start);
            graph.parent.push_back(no_index);
            graph.successors.emplace_back();

            for (std::size_t current = 0; current < graph.states.size(); current++)
            {
                KIterator successors = kripke.successors(graph.states[current]);
                for (successors.first(); !successors.done(); successors.next())
                {
                    const KState next = successors.state();
                    if (!allowed(next))
                        continue;

                    const auto found = index.find(next);
                    std::size_t target;
                    if (found == index.end())
                    {
                        target = graph.states.size();
                        index.emplace(next, target);
                        graph.states.push_back(next);
                        graph.parent.push_back(current);
                        graph.successors.emplace_back();
                    }
                    else
                    {
                        target = found->second;
                    }
                    graph.successors[current].push_back(target);
                }
            }
            return graph;
        }

        //! Finds a cycle of \a stays states, as (prefix, cycle) index lists.
        /**
         * Depth first over the stored graph, colouring nodes white/gray/black:
         * an edge to a gray node closes a cycle, and the gray node is an
         * ancestor of the current one in the DFS tree, so the cycle is the
         * ancestor chain plus that edge. The prefix comes from the
         * breadth-first tree, i.e. it is a shortest path to the cycle entry.
         *
         * The prefix may pass through states outside \a stays - it only has to
         * reach the cycle - which is what makes this the `F(G(P))` search.
         * Iterative on purpose: a straight-line program is tens of thousands of
         * states deep and the recursive form would overflow the stack.
         *
         * \param graph an explored graph, e.g. from `explore()`
         * \param stays predicate every state of the cycle has to satisfy
         * \return (prefix indices, cycle indices) or no value when no such
         *         cycle exists
         */
        std::optional<std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
        find_cycle(const Graph &graph, const StatePredicate &stays)
        {
            const std::size_t count = graph.states.size();
            std::vector<bool> allowed(count, false);
            for (std::size_t at = 0; at < count; at++)
                allowed[at] = stays(graph.states[at]);

            std::vector<unsigned char> colour(count, white);
            std::vector<std::size_t> tree_parent(count, no_index);
            // (node, index of the next successor to try)
            std::vector<std::pair<std::size_t, std::size_t>> stack;

            for (std::size_t root = 0; root < count; root++)
            {
                if (!allowed[root] || colour[root] != white)
                    continue;
                colour[root] = gray;
                stack.emplace_back(root, 0);

                while (!stack.empty())
                {
                    const std::size_t node = stack.back().first;
                    bool descended = false;
                    while (stack.back().second < graph.successors[node].size())
                    {
                        const std::size_t child = graph.successors[node][stack.back().second++];
                        if (!allowed[child])
                            continue;
                        if (colour[child] == gray)
                        {
                            // The edge closes a cycle: `child` is an ancestor of
                            // `node`, so the cycle is child..node plus this edge.
                            std::vector<std::size_t> cycle;
                            for (std::size_t at = node; at != child; at = tree_parent[at])
                                cycle.push_back(at);
                            cycle.push_back(child);
                            std::reverse(cycle.begin(), cycle.end());

                            std::vector<std::size_t> prefix;
                            for (std::size_t at = graph.parent[child]; at != no_index; at = graph.parent[at])
                                prefix.push_back(at);
                            std::reverse(prefix.begin(), prefix.end());
                            return std::make_pair(prefix, cycle);
                        }
                        if (colour[child] == white)
                        {
                            colour[child] = gray;
                            tree_parent[child] = node;
                            stack.emplace_back(child, 0);
                            descended = true;
                            break;
                        }
                        // Black: fully explored, so it cannot close a cycle.
                    }
                    if (!descended)
                    {
                        colour[node] = black;
                        stack.pop_back();
                    }
                }
            }
            return std::nullopt;
        }

        //! Turns index lists into the states they name.
        Run make_run(const Graph &graph, const std::vector<std::size_t> &prefix,
                     const std::vector<std::size_t> &cycle)
        {
            Run run;
            run.prefix.reserve(prefix.size());
            for (const std::size_t at : prefix)
                run.prefix.push_back(graph.states[at]);
            run.cycle.reserve(cycle.size());
            for (const std::size_t at : cycle)
                run.cycle.push_back(graph.states[at]);
            return run;
        }
    }

    std::optional<Run> search_eventually(const Kripke &kripke, const StatePredicate &holds)
    {
        std::unordered_map<KState, std::size_t, KStateHash, KStateEqual> index;
        std::vector<KState> states;
        std::vector<std::size_t> parent;

        const KState start = kripke.initial_state();
        index.emplace(start, 0);
        states.push_back(start);
        parent.push_back(no_index);

        // Decides `E F(P)` (= the counterexample of `A G(!P)`).
        //
        // Breadth first, so the first state that satisfies the atom is reached
        // by a shortest path; the search stops there and never looks at the
        // rest of the graph. A path is already a witness: every state has a
        // successor, so it extends to an infinite run.
        std::size_t target = no_index;
        for (std::size_t current = 0; current < states.size() && target == no_index; current++)
        {
            if (holds(states[current]))
            {
                target = current;
                break;
            }

            KIterator successors = kripke.successors(states[current]);
            for (successors.first(); !successors.done(); successors.next())
            {
                const KState next = successors.state();
                const auto found = index.find(next);
                if (found != index.end())
                    continue;
                index.emplace(next, states.size());
                states.push_back(next);
                parent.push_back(current);
            }
        }

        if (target == no_index)
            return std::nullopt;

        Run run;
        for (std::size_t at = target; at != no_index; at = parent[at])
            run.prefix.push_back(states[at]);
        std::reverse(run.prefix.begin(), run.prefix.end());
        return run;
    }

    std::optional<Run> search_always(const Kripke &kripke, const StatePredicate &holds)
    {
        // Decides `E G(P)` (= the counterexample of `A F(!P)`).
        //
        // Depth first and restricted to the atom subgraph: a state that leaves
        // the subgraph ends the run, so it is never explored - that is what
        // makes the prefix part of the requirement. An infinite path in a
        // finite subgraph contains a cycle and a cycle can be pumped, so a back
        // edge is exactly the lasso we are looking for. The search stops at the
        // first back edge, which is why a counterexample close to the initial
        // state does not pay for the rest of the graph.
        std::unordered_map<KState, std::size_t, KStateHash, KStateEqual> index;
        std::vector<KState> states;
        std::vector<std::size_t> tree_parent;
        std::vector<unsigned char> colour;

        const KState start = kripke.initial_state();
        if (!holds(start))
            return std::nullopt;

        index.emplace(start, 0);
        states.push_back(start);
        tree_parent.push_back(no_index);
        colour.push_back(gray);

        // (state index, iterator over its successors)
        struct Frame
        {
            std::size_t node;
            KIterator successors;
        };
        std::vector<Frame> stack;
        stack.push_back(Frame{0, kripke.successors(states[0])});
        stack.back().successors.first();

        while (!stack.empty())
        {
            const std::size_t node = stack.back().node;
            bool descended = false;
            while (!stack.back().successors.done())
            {
                const KState next = stack.back().successors.state();
                stack.back().successors.next();
                if (!holds(next))
                    continue;

                const auto found = index.find(next);
                if (found == index.end())
                {
                    const std::size_t child = states.size();
                    index.emplace(next, child);
                    states.push_back(next);
                    tree_parent.push_back(node);
                    colour.push_back(gray);
                    stack.push_back(Frame{child, kripke.successors(states[child])});
                    stack.back().successors.first();
                    descended = true;
                    break;
                }

                if (colour[found->second] == gray)
                {
                    // `next` is on the current DFS path, so the tree path from
                    // it to `node` plus this edge is a cycle of atom states.
                    const std::size_t child = found->second;
                    Run run;
                    for (std::size_t at = node; at != child; at = tree_parent[at])
                        run.cycle.push_back(states[at]);
                    run.cycle.push_back(states[child]);
                    std::reverse(run.cycle.begin(), run.cycle.end());
                    for (std::size_t at = tree_parent[child]; at != no_index; at = tree_parent[at])
                        run.prefix.push_back(states[at]);
                    std::reverse(run.prefix.begin(), run.prefix.end());
                    return run;
                }
                // Black: fully explored, so no cycle can close into it.
            }

            if (!descended)
            {
                colour[node] = black;
                stack.pop_back();
            }
        }
        return std::nullopt;
    }

    std::optional<Run> search_eventually_always(const Kripke &kripke, const StatePredicate &holds)
    {
        // Decides `E F(G(P))` (= the counterexample of `A G(F(!P))`).
        //
        // The prefix may pass through states that leave the atom set (`F` only
        // asks for the suffix), so the whole reachable graph is needed before
        // the cycle inside the atom set can be looked for.
        const Graph graph = explore(kripke, [](const KState &) { return true; });
        const auto found = find_cycle(graph, holds);
        if (!found.has_value())
            return std::nullopt;
        return make_run(graph, found->first, found->second);
    }
}
