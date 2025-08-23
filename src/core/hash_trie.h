#ifndef FLY_CORE_HASH_TRIE_H
#define FLY_CORE_HASH_TRIE_H

#include "arena.h"
#include "assert.h"
#include "hash.h"
#include "memory.h"

namespace Fly
{

template <typename KeyType, typename ValueType>
struct HashTrie
{
    struct Node
    {
        Node* children[4];
        KeyType key;
        ValueType value;
    };

    ValueType* Find(const KeyType& key)
    {
        Node* node = root_;
        Hash<KeyType> hashFunc;
        u64 h = hashFunc(key);
        u8 shift = 0;
        do
        {
            if (!node)
            {
                return nullptr;
            }

            if (key == node->key)
            {
                return &(node->value);
            }

            node = node->children[h & 3];
            h >>= 2;
            shift += 2;
        } while (shift < 64);

        return nullptr;
    }

    const ValueType* Find(const KeyType& key) const
    {
        const Node* node = root_;
        Hash<KeyType> hashFunc;
        u64 h = hashFunc(key);
        u8 shift = 0;
        do
        {
            if (!node)
            {
                return nullptr;
            }

            if (key == node->key)
            {
                return &(node->value);
            }

            node = node->children[h & 3];
            h >>= 2;
            shift += 2;
        } while (shift < 64);

        return nullptr;
    }

    ValueType& Insert(Arena& arena, const KeyType& key,
                      const ValueType& value = ValueType())
    {
        Node** node = &root_;
        Hash<KeyType> hashFunc;

        u64 h = hashFunc(key);
        u8 shift = 0;
        do
        {
            if (!*node)
            {
                *node = FLY_PUSH_ARENA(arena, Node, 1);
                (*node)->key = key;
                (*node)->value = value;
                Fly::MemZero((*node)->children, sizeof(Node*) * 4);
                count_++;
                return (*node)->value;
            }

            if (key == (*node)->key)
            {
                (*node)->value = value;
                return (*node)->value;
            }

            node = &(*node)->children[h & 3];
            h >>= 2;
            shift += 2;
        } while (shift < 64);

        // TODO: Handle hash collisions, if will ever happen
        FLY_ASSERT(false);
        return (*node)->value;
    }

    inline u64 Count() const { return count_; }

    struct Iterator
    {
        struct StackEntry
        {
            Node* node;
            u32 childIndex;
        };

        explicit Iterator(Node* root)
        {
            if (root)
            {
                stack_[0] = {root, 0};
                depth_ = 1;
                current_ = root;
            }
            else
            {
                depth_ = 0;
                current_ = nullptr; // end iterator
            }
        }

        Iterator() : current_(nullptr), depth_(0) {}

        Node* operator*() const { return current_; }

        Iterator& operator++()
        {
            Advance();
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return current_ != other.current_;
        }

    private:
        void Advance()
        {
            if (!current_)
            {
                return;
            }

            while (depth_ > 0)
            {
                StackEntry& top = stack_[depth_ - 1];

                if (top.childIndex < 4)
                {
                    // Try next child
                    Node* child = top.node->children[top.childIndex];
                    top.childIndex++;

                    if (child)
                    {
                        if (depth_ == 32)
                        {
                            current_ = nullptr;
                            depth_ = 0;
                            return;
                        }

                        stack_[depth_] = {child, 0};
                        depth_++;
                        current_ = child;
                        return;
                    }
                }
                else
                {
                    depth_--;
                }
            }

            current_ = nullptr;
        }

        StackEntry stack_[32];
        u32 depth_ = 0;
        Node* current_ = nullptr;
    };

    struct ConstIterator
    {
        struct StackEntry
        {
            const Node* node;
            u32 childIndex;
        };

        explicit ConstIterator(const Node* root)
        {
            if (root)
            {
                stack_[0] = {root, 0};
                depth_ = 1;
                current_ = root;
            }
            else
            {
                depth_ = 0;
                current_ = nullptr; // end iterator
            }
        }

        ConstIterator() : current_(nullptr), depth_(0) {}

        const Node* operator*() const { return current_; }

        ConstIterator& operator++()
        {
            Advance();
            return *this;
        }

        bool operator!=(const ConstIterator& other) const
        {
            return current_ != other.current_;
        }

    private:
        void Advance()
        {
            if (!current_)
            {
                return;
            }

            while (depth_ > 0)
            {
                StackEntry& top = stack_[depth_ - 1];

                if (top.childIndex < 4)
                {
                    // Try next child
                    const Node* child = top.node->children[top.childIndex];
                    top.childIndex++;

                    if (child)
                    {
                        if (depth_ == 32)
                        {
                            current_ = nullptr;
                            depth_ = 0;
                            return;
                        }

                        stack_[depth_] = {child, 0};
                        depth_++;
                        current_ = child;
                        return;
                    }
                }
                else
                {
                    depth_--;
                }
            }

            current_ = nullptr;
        }

        StackEntry stack_[32];
        u32 depth_ = 0;
        const Node* current_ = nullptr;
    };

    Iterator begin() { return Iterator(root_); }
    Iterator end() { return Iterator(nullptr); }

    ConstIterator begin() const { return ConstIterator(root_); }
    ConstIterator end() const { return ConstIterator(nullptr); }

private:
    Node* root_ = nullptr;
    u64 count_ = 0;
};

} // namespace Fly

#endif /* FLY_CORE_HASH_TRIE_H */
