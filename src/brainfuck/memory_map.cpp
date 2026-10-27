#include <cstdint>
#include <utility>
#include "memory_map.hpp"

namespace brainfuck
{
    std::size_t mix_hash(std::size_t value)
    {
        std::uint64_t mixed = static_cast<std::uint64_t>(value);
        mixed += 0x9e3779b97f4a7c15ULL;
        mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ULL;
        mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebULL;
        return static_cast<std::size_t>(mixed ^ (mixed >> 31));
    }

    namespace
    {
        //! Priority of a cell in the treap; only the relative order matters.
        std::size_t priority_of(mem_ptr_t cell)
        {
            return mix_hash(static_cast<std::size_t>(cell));
        }

        //! Contribution of a single cell to the hash of a snapshot.
        std::size_t entry_hash(mem_ptr_t cell, cell_value_t value)
        {
            return mix_hash(static_cast<std::size_t>(cell) ^ mix_hash(static_cast<std::size_t>(value) + 0x9e3779b97f4a7c15ULL));
        }
    }

    //! One immutable treap node, shared by every snapshot that contains it.
    struct MemoryMap::Node
    {
        //! Nodes are immutable and shared between snapshots; the const members
        //! turn any attempt to move a child out of a shared node into a copy.
        Node(mem_ptr_t cell, cell_value_t value, std::shared_ptr<Node> left, std::shared_ptr<Node> right)
            : cell(cell),
              value(value),
              priority(priority_of(cell)),
              left(std::move(left)),
              right(std::move(right)),
              subtree_size(1 + size_of(this->left) + size_of(this->right)),
              subtree_hash(hash_of(this->left) ^ hash_of(this->right) ^ entry_hash(cell, value))
        {
        }

        const mem_ptr_t cell;              //!< tape index this node stores
        const cell_value_t value;          //!< non-zero cell value
        const std::size_t priority;        //!< treap heap priority, derived from cell
        const std::shared_ptr<Node> left;  //!< subtree of smaller cells
        const std::shared_ptr<Node> right; //!< subtree of larger cells
        const std::size_t subtree_size;    //!< cached number of cells in this subtree
        const std::size_t subtree_hash;    //!< cached XOR hash of this subtree
    };

    MemoryMap::MemoryMap() = default;

    MemoryMap::MemoryMap(node_ptr root) : root(std::move(root))
    {
    }

    MemoryMap::MemoryMap(const value_map_t &values)
    {
        for (const auto &entry : values)
            if (entry.second != 0)
                root = insert(std::move(root), entry.first, entry.second);
    }

    bool MemoryMap::empty() const
    {
        return this->root == nullptr;
    }

    std::size_t MemoryMap::size() const
    {
        return size_of(this->root);
    }

    cell_value_t MemoryMap::get(mem_ptr_t cell) const
    {
        const Node *node = find(this->root.get(), cell);
        return node == nullptr ? 0 : node->value;
    }

    MemoryMap MemoryMap::set(mem_ptr_t cell, cell_value_t value) const
    {
        if (value == 0)
        {
            node_ptr pruned = erase(this->root, cell);
            if (pruned == this->root)
                return *this;
            return MemoryMap(std::move(pruned));
        }

        const Node *existing = find(this->root.get(), cell);
        if (existing != nullptr)
        {
            if (existing->value == value)
                return *this;
            return MemoryMap(replace(this->root, cell, value));
        }

        return MemoryMap(insert(this->root, cell, value));
    }

    std::size_t MemoryMap::hash() const
    {
        return hash_of(this->root);
    }

    int MemoryMap::compare(const MemoryMap &other) const
    {
        if (this->root == other.root)
            return 0;

        // Ordering by size and then by hash first keeps the entry-wise walk -
        // the expensive part - off everything but genuine hash collisions.
        const std::size_t this_size = size();
        const std::size_t other_size = other.size();
        if (this_size != other_size)
            return this_size < other_size ? -1 : 1;

        const std::size_t this_hash = hash();
        const std::size_t other_hash = other.hash();
        if (this_hash != other_hash)
            return this_hash < other_hash ? -1 : 1;

        std::vector<const Node *> this_stack;
        std::vector<const Node *> other_stack;
        push_left(this->root, this_stack);
        push_left(other.root, other_stack);
        while (!this_stack.empty() && !other_stack.empty())
        {
            const Node *this_node = this_stack.back();
            this_stack.pop_back();
            const Node *other_node = other_stack.back();
            other_stack.pop_back();

            if (this_node->cell != other_node->cell)
                return this_node->cell < other_node->cell ? -1 : 1;
            if (this_node->value != other_node->value)
                return this_node->value < other_node->value ? -1 : 1;

            push_left(this_node->right, this_stack);
            push_left(other_node->right, other_stack);
        }

        // Same size and same hash, so the walks end together: equal contents.
        return 0;
    }

    bool MemoryMap::operator==(const MemoryMap &other) const
    {
        return compare(other) == 0;
    }

    bool MemoryMap::operator!=(const MemoryMap &other) const
    {
        return !(*this == other);
    }

    MemoryMap::value_map_t MemoryMap::to_std_map() const
    {
        value_map_t values;
        std::vector<const Node *> stack;
        push_left(this->root, stack);
        while (!stack.empty())
        {
            const Node *node = stack.back();
            stack.pop_back();
            values.emplace(node->cell, node->value);
            push_left(node->right, stack);
        }
        return values;
    }

    MemoryMap::node_ptr MemoryMap::make_node(mem_ptr_t cell, cell_value_t value, node_ptr left, node_ptr right)
    {
        return std::make_shared<Node>(cell, value, std::move(left), std::move(right));
    }

    MemoryMap::node_ptr MemoryMap::insert(node_ptr tree, mem_ptr_t cell, cell_value_t value)
    {
        node_ptr left;
        node_ptr right;
        split(std::move(tree), cell, left, right);
        node_ptr node = make_node(cell, value, nullptr, nullptr);
        return merge(merge(std::move(left), std::move(node)), std::move(right));
    }

    MemoryMap::node_ptr MemoryMap::replace(node_ptr tree, mem_ptr_t cell, cell_value_t value)
    {
        if (cell < tree->cell)
            return make_node(tree->cell, tree->value, replace(tree->left, cell, value), tree->right);
        if (tree->cell < cell)
            return make_node(tree->cell, tree->value, tree->left, replace(tree->right, cell, value));
        return make_node(cell, value, tree->left, tree->right);
    }

    MemoryMap::node_ptr MemoryMap::erase(node_ptr tree, mem_ptr_t cell)
    {
        if (tree == nullptr)
            return nullptr;
        if (cell < tree->cell)
        {
            node_ptr left = erase(tree->left, cell);
            if (left == tree->left)
                return tree;
            return make_node(tree->cell, tree->value, std::move(left), tree->right);
        }
        if (tree->cell < cell)
        {
            node_ptr right = erase(tree->right, cell);
            if (right == tree->right)
                return tree;
            return make_node(tree->cell, tree->value, tree->left, std::move(right));
        }
        // NODES ARE SHARED: never move a child out of a node that other
        // snapshots may still reference.
        return merge(tree->left, tree->right);
    }

    MemoryMap::node_ptr MemoryMap::merge(node_ptr left, node_ptr right)
    {
        if (left == nullptr)
            return right;
        if (right == nullptr)
            return left;
        if (left->priority > right->priority)
            return make_node(left->cell, left->value, left->left, merge(left->right, std::move(right)));
        return make_node(right->cell, right->value, merge(std::move(left), right->left), right->right);
    }

    void MemoryMap::split(node_ptr tree, mem_ptr_t cell, node_ptr &left, node_ptr &right)
    {
        if (tree == nullptr)
        {
            left = nullptr;
            right = nullptr;
            return;
        }

        if (tree->cell < cell)
        {
            node_ptr lower;
            node_ptr greater;
            split(tree->right, cell, lower, greater);
            left = make_node(tree->cell, tree->value, tree->left, std::move(lower));
            right = std::move(greater);
        }
        else
        {
            node_ptr lower;
            node_ptr greater;
            split(tree->left, cell, lower, greater);
            left = std::move(lower);
            right = make_node(tree->cell, tree->value, std::move(greater), tree->right);
        }
    }

    const MemoryMap::Node *MemoryMap::find(const Node *tree, mem_ptr_t cell)
    {
        while (tree != nullptr)
        {
            if (cell < tree->cell)
                tree = tree->left.get();
            else if (tree->cell < cell)
                tree = tree->right.get();
            else
                return tree;
        }
        return nullptr;
    }

    std::size_t MemoryMap::size_of(const node_ptr &tree)
    {
        return tree == nullptr ? 0 : tree->subtree_size;
    }

    std::size_t MemoryMap::hash_of(const node_ptr &tree)
    {
        return tree == nullptr ? 0 : tree->subtree_hash;
    }

    void MemoryMap::push_left(const node_ptr &tree, std::vector<const Node *> &stack)
    {
        for (const Node *node = tree.get(); node != nullptr; node = node->left.get())
            stack.push_back(node);
    }
}
