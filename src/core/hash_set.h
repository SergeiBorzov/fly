#ifndef HLS_CORE_HASH_SET_H
#define HLS_CORE_HASH_SET_H

#include "arena.h"
#include "assert.h"
#include "hash.h"
#include "memory.h"

namespace Hls
{

template <typename T>
struct HashSet
{
    struct Node
    {
        Node* children[4];
        T value;
    };

    bool Has(const T& value)
    {
        Node** node = &root_;
        Hash<T> hashFunc;

        u64 h = hashFunc(value);
        do
        {
            if (!*node)
            {
                return false;
            }

            if (value == (*node)->value)
            {
                return true;
            }

            node = &(*node)->children[h & 3];
            h >>= 2;
            h = hashFunc(value);
        } while (h != 0);

        return false;
    }

    void Insert(Arena& arena, const T& value)
    {
        Node** node = &root_;
        Hash<T> hashFunc;

        u64 h = hashFunc(value);
        do
        {
            if (!*node)
            {
                *node = HLS_ALLOC(arena, Node, 1);
                (*node)->value = value;
                Hls::MemZero((*node)->children, sizeof(Node*) * 4);
                count_++;
                return;
            }

            if (value == (*node)->value)
            {
                return;
            }

            node = &(*node)->children[h & 3];
            h >>= 2;
            h = hashFunc(value);
        } while (h != 0);

        // TODO: Handle hash collisions, if will ever happen
        HLS_ASSERT(false);
        return;
    }

    inline u64 Count() const { return count_; }

private:
    Node* root_ = nullptr;
    u64 count_ = 0;
};

} // namespace Hls

#endif /* HLS_CORE_HASH_SET_H */
