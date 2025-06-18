#ifndef FLY_DEMOS_OCEAN_QUAD_TREE_H
#define FLY_DEMOS_OCEAN_QUAD_TREE_H

#include "core/arena.h"
#include "math/vec.h"
#include "rhi/buffer.h"
#include "rhi/device.h"

namespace Fly
{

// This QuadTree is special and only accounts for one point (camera position)
class QuadTree
{
public:
    struct Node
    {
        Math::Vec4 posSize;
    };

    bool Create(Arena& arena, RHI::Device& device, u32 sizeExp, u32 minSizeExp,
                Math::Vec2 position);
    void Update(Arena& arena, Math::Vec2 position);
    void Destroy(RHI::Device& device);

    inline const Node* GetNodes() const { return nodes_; }
    inline u32 GetNodeCount() const { return nodeCount_; }

    RHI::Buffer quadBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
private:
    Node* nodes_ = nullptr;
    u32 nodeCount_ = 0;
    u32 totalNodeCount_ = 0;
    f32 minSize_ = 0;
};

} // namespace Fly

#endif /* FLY_DEMOS_OCEAN_QUAD_TREE_H */
