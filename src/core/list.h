#ifndef FLY_CORE_LIST_H
#define FLY_CORE_LIST_H

#include "arena.h"

namespace Fly
{

template <typename T>
struct List
{
    struct Node
    {
        T data;
        Node* next;
    };

    void InsertFront(Arena& arena, const T& value)
    {
        Node* node = FLY_PUSH_ARENA(arena, Node, 1);
        node->next = head_;
        node->data = value;

        head_ = node;

        if (tail_ == nullptr)
        {
            tail_ = head_;
        }
    }

    void InsertBack(Arena& arena, const T& value)
    {
        Node* node = FLY_PUSH_ARENA(arena, Node, 1);
        node->next = nullptr;
        node->data = value;

        if (tail_)
        {
            tail_->next = node;
        }
        tail_ = node;

        if (head_ == nullptr)
        {
            head_ = tail_;
        }

        return;
    }

    void Remove(const T& value)
    {
        Node** curr = &head_;

        while (*curr)
        {
            if ((*curr)->data == value)
            {
                Node* node = *curr;
                *curr = (*curr)->next;
                return;
            }
            curr = &((*curr)->next);
        }
    }

    class Iterator
    {
        Node* current;

    public:
        explicit Iterator(Node* node) : current(node) {}

        T& operator*() const { return current->data; }

        Iterator& operator++()
        {
            current = current->next;
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            return current == other.current;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }
    };

    class ConstIterator
    {
        const Node* current;

    public:
        explicit ConstIterator(const Node* node) : current(node) {}

        const T& operator*() const { return current->data; }

        ConstIterator& operator++()
        {
            current = current->next;
            return *this;
        }

        bool operator==(const ConstIterator& other) const
        {
            return current == other.current;
        }

        bool operator!=(const ConstIterator& other) const
        {
            return !(*this == other);
        }
    };

    Iterator begin() { return Iterator(head_); }
    Iterator end() { return Iterator(nullptr); }

    ConstIterator cbegin() const { return ConstIterator(head_); }
    ConstIterator cend() const { return ConstIterator(nullptr); }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
};

} // namespace Fly

#endif /* FLY_CORE_LIST_H */
