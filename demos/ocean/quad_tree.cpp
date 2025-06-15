#include "quad_tree.h"

namespace Fly
{

QuadTree::QuadTree(Math::Vec2 position, f32 size)
    : position_(position), size_(size), depth_(0)
{
    for (u32 i = 0; i < 4; i++)
    {
        nodes_[i] = nullptr;
    }
}

void QuadTree::Update(Arena& arena, Math::Vec2 position)
{
    Math::Vec2 diff = position - position_;
    diff.x = Math::Abs(diff.x);
    diff.y = Math::Abs(diff.y);

    if (diff.x < size_ && diff.y < size_ && size_ > MIN_QUAD_TREE_NODE_SIZE)
    {
        Split(arena);
        for (u32 i = 0; i < 4; i++)
        {
            Update(arena, position);
        }
    }
    else if (diff.x > size_ || diff.y > size_)
    {
        Collapse();
    }
}

void QuadTree::Split(Arena& arena)
{
    QuadTree* nodes = FLY_PUSH_ARENA(arena, QuadTree, 4);

    Math::Vec2 offsets[4] = {Math::Vec2(-1.0f, 1.0f), Math::Vec2(1.0f, 1.0f),
                             Math::Vec2(-1.0f, -1.0f), Math::Vec2(1.0f, -1.0f)};
    for (u32 i = 0; i < 4; i++)
    {
        nodes_[i] = &nodes[i];
        nodes_[i]->size_ = size_ * 0.5f;
        nodes_[i]->position_ = position_ + offsets[4] * size_ * 0.25f;
    }
}

void QuadTree::Collapse()
{
    for (u32 i = 0; i < 4; i++)
    {
        if (nodes_[i])
        {
            nodes_[i]->Collapse();
        }
        nodes_[i] = nullptr;
    }
}

} // namespace Fly
