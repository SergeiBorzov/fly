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
        Node** node = &root_;
        Hash<KeyType> hashFunc;
        u64 h = hashFunc(key);
        do
        {
            if (!*node)
            {
                return nullptr;
            }

            if (key == (*node)->key)
            {
                return &(*node)->value;
            }

            node = &(*node)->children[h & 3];
            h >>= 2;
            h = hashFunc(key);
        } while (h != 0);

        return nullptr;
    }

    ValueType& Insert(Arena& arena, const KeyType& key,
                      const ValueType& value = ValueType())
    {
        Node** node = &root_;
        Hash<KeyType> hashFunc;

        u64 h = hashFunc(key);
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
                return (*node)->value;
            }

            node = &(*node)->children[h & 3];
        } while (h != 0);

        // TODO: Handle hash collisions, if will ever happen
        FLY_ASSERT(false);
        return (*node)->value;
    }

    inline u64 Count() const { return count_; }

private:
    Node* root_ = nullptr;
    u64 count_ = 0;
};

} // namespace Fly

#endif /* FLY_CORE_HASH_TRIE_H */
