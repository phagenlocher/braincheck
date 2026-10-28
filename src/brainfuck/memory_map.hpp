#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <vector>
#include "model.hpp"

namespace brainfuck
{
    //! SplitMix64 finalizer used by the state and memory hashes.
    /**
     * Mixing the raw values matters: the search keys its state table on
     * hash(), and the inputs here (instruction pointers, cell indices) are
     * small consecutive integers that std::hash leaves untouched.
     */
    std::size_t mix_hash(std::size_t value);

    //! Immutable sparse memory tape, shared between Kripke states.
    /**
     * Every Kripke transition produces a new memory snapshot and every
     * snapshot outlives the transition that created it, because the search
     * keeps states in its visited table. A std::map therefore has to be copied for
     * every successor, which makes exploration quadratic in the number of
     * cells touched.
     *
     * MemoryMap is a persistent map instead: it is a treap whose nodes are
     * never mutated. Copying a MemoryMap is O(1), and setting one cell
     * allocates O(log n) nodes that share all other nodes with the previous
     * snapshot.
     *
     * A cell holding zero is canonically absent, mirroring the rule that makes
     * KState comparison and hashing agree.
     *
     * Priorities and entry hashes are deterministic functions of the cell
     * index, so two maps with equal contents always have the same shape and
     * the same hash no matter in which order they were built.
     */
    class MemoryMap
    {
    public:
        //! Plain representation, used for interoperability and debugging.
        using value_map_t = std::map<mem_ptr_t, cell_value_t>;

        //! Empty snapshot; every cell reads as zero.
        MemoryMap();
        //! Builds a map from a plain representation, dropping zero cells.
        explicit MemoryMap(const value_map_t &values);

        //! True when no cell is present, i.e. every cell reads as zero.
        bool empty() const;
        //! Number of non-zero cells.
        std::size_t size() const;
        //! Value of \a cell, or zero when the cell is absent.
        cell_value_t get(mem_ptr_t cell) const;
        //! Snapshot with \a cell set to \a value; a zero value removes it.
        /**
         * Returns \c *this unchanged when the snapshot already has the
         * requested contents, so unchanged successors share the whole tree.
         */
        MemoryMap set(mem_ptr_t cell, cell_value_t value) const;

        //! Order-independent hash of the contents, cached per snapshot.
        std::size_t hash() const;
        //! Strict weak ordering that is consistent with operator==.
        /**
         * Compares by size, then by contents hash, then cell by cell, so equal
         * contents always compare equal whatever order built them.
         */
        int compare(const MemoryMap &other) const;

        //! True when both snapshots hold the same cells and values.
        bool operator==(const MemoryMap &other) const;
        //! Negation of operator==.
        bool operator!=(const MemoryMap &other) const;

        //! Materialises the contents as a plain ordered map; for tests and debugging.
        value_map_t to_std_map() const;

    private:
        //! One node of the persistent treap; immutable once constructed.
        struct Node;
        //! Shared, never-mutated pointer to a node.
        using node_ptr = std::shared_ptr<Node>;

        //! Root of the treap, or nullptr for the empty map.
        node_ptr root;

        //! Wraps an existing tree without touching its nodes.
        explicit MemoryMap(node_ptr root);

        //! Allocates a node and recomputes its cached size and hash.
        static node_ptr make_node(mem_ptr_t cell, cell_value_t value, node_ptr left, node_ptr right);
        //! Inserts a cell that is known to be absent.
        static node_ptr insert(node_ptr tree, mem_ptr_t cell, cell_value_t value);
        //! Updates the value of a cell that is known to be present.
        static node_ptr replace(node_ptr tree, mem_ptr_t cell, cell_value_t value);
        //! Removes \a cell, returning the original tree when it is absent.
        static node_ptr erase(node_ptr tree, mem_ptr_t cell);
        //! Treap union: all cells of \a left and \a right, sharing unchanged subtrees.
        static node_ptr merge(node_ptr left, node_ptr right);
        //! Splits \a tree into cells below \a cell and cells at or above it.
        /**
         * Path copying: the input tree is left untouched, so snapshots built
         * from it stay valid.
         */
        static void split(node_ptr tree, mem_ptr_t cell, node_ptr &left, node_ptr &right);
        //! Finds \a cell without allocating; nullptr when it is absent.
        static const Node *find(const Node *tree, mem_ptr_t cell);
        //! Cached number of cells in \a tree.
        static std::size_t size_of(const node_ptr &tree);
        //! Cached order-independent hash of \a tree.
        static std::size_t hash_of(const node_ptr &tree);
        //! Pushes the left spine of \a tree onto \a stack for an in-order walk.
        static void push_left(const node_ptr &tree, std::vector<const Node *> &stack);
    };
}
