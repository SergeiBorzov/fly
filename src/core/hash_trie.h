#ifndef HLS_CORE_HASH_TRIE_H
#define HLS_CORE_HASH_TRIE_H

#include "arena.h"
#include "assert.h"
#include "hash.h"
#include "memory.h"

namespace Hls
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
        for (u64 h = Hash<KeyType>(key); h != 0; h >>= 2)
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
        }

        return nullptr;
    }

    ValueType& Insert(Arena& arena, const KeyType& key, const ValueType& value = ValueType())
    {
        Node** node = &root_;
        Hash<KeyType> hashFunc;
        for (u64 h = hashFunc(key); h != 0; h >>= 2)
        {
            if (!*node)
            {
                *node = HLS_ALLOC(arena, Node, 1);
                (*node)->key = key;
                (*node)->value = value;
                Hls::MemZero((*node)->children, sizeof(Node*) * 4);
                return (*node)->value;
            }

            if (key == (*node)->key)
            {
                return (*node)->value;
            }

            node = &(*node)->children[h & 3];
        }

        // TODO: Handle hash collisions, if will ever happen
        HLS_ASSERT(false);
        return (*node)->value;
    }
private:
    Node* root_ = nullptr;
};

} // namespace Hls

#endif /* HLS_CORE_HASH_TRIE_H */
