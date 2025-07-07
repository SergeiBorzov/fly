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

    inline u64 Count() const { return count_; }

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
        count_++;
    }

    inline T* Head()
    {
        if (!head_)
        {
            return nullptr;
        }
        return &(head_->data);
    }

    inline const T* Head() const
    {
        if (!head_)
        {
            return nullptr;
        }
        return &(head_->data);
    }

    void PopFront()
    {
        if (head_)
        {
            head_ = head_->next;
            if (!head_)
            {
                tail_ = nullptr;
            }
            count_--;
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
        count_++;
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

    struct Iterator
    {
        explicit Iterator(Node* node) : current_(node) {}

        T& operator*() const { return current_->data; }

        Iterator& operator++()
        {
            current_ = current_->next;
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            return current_ == other.current_;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

    private:
        Node* current_;
    };

    struct ConstIterator
    {
        explicit ConstIterator(const Node* node) : current_(node) {}

        const T& operator*() const { return current_->data; }

        ConstIterator& operator++()
        {
            current_ = current_->next;
            return *this;
        }

        bool operator==(const ConstIterator& other) const
        {
            return current_ == other.current_;
        }

        bool operator!=(const ConstIterator& other) const
        {
            return !(*this == other);
        }

    private:
        const Node* current_;
    };

    Iterator begin() { return Iterator(head_); }
    Iterator end() { return Iterator(nullptr); }

    ConstIterator begin() const { return ConstIterator(head_); }
    ConstIterator end() const { return ConstIterator(nullptr); }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    u64 count_ = 0;
};

} // namespace Fly

#endif /* FLY_CORE_LIST_H */
