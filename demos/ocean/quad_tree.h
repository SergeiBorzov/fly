#ifndef FLY_DEMOS_OCEAN_QUAD_TREE_H
#define FLY_DEMOS_OCEAN_QUAD_TREE_H

#include "core/arena.h"
#include "math/vec.h"

#define MIN_QUAD_TREE_NODE_SIZE 16.0f

namespace Fly
{

class QuadTree
{
public:
    QuadTree(Math::Vec2 position, f32 size);
    void Update(Arena& arena, Math::Vec2 position);
    void Split(Arena& arena);
    void Collapse();
private:
    QuadTree* nodes_[4];
    Math::Vec2 position_;
    f32 size_;
    u32 depth_;
};

} // namespace Fly

#endif /* FLY_DEMOS_OCEAN_QUAD_TREE_H */
