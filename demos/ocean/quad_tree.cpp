#include "quad_tree.h"

#include "core/log.h"

namespace Fly
{

bool QuadTree::Create(Arena& arena, RHI::Device& device, u32 sizeExp,
                      u32 minSizeExp, Math::Vec2 position)
{
    FLY_ASSERT(sizeExp);
    FLY_ASSERT(minSizeExp);
    FLY_ASSERT(minSizeExp <= sizeExp);

    minSize_ = static_cast<f32>(1U << minSizeExp);
    f32 size = static_cast<f32>(1U << sizeExp);
    u32 depth = sizeExp - minSizeExp;

    totalNodeCount_ = 1 + 4 * depth;

    nodes_ = FLY_PUSH_ARENA(arena, Node, totalNodeCount_);

    nodes_[0].posSize = Math::Vec4(0.0f, 0.0f, size, 0.0f);
    nodeCount_ = 1;

    Math::Vec2 offsets[4] = {Math::Vec2(-1.0f, -1.0f), Math::Vec2(-1.0f, 1.0f),
                             Math::Vec2(1.0f, 1.0f), Math::Vec2(1.0f, -1.0f)};

    for (u32 i = 0; i < depth; i++)
    {
        if (i == 0)
        {
            Node& node = nodes_[0];
            Math::Vec2 center = Math::Vec2(node.posSize);
            f32 size = node.posSize.z;

            Math::Vec2 diff = position - center;
            diff.x = Math::Abs(diff.x);
            diff.y = Math::Abs(diff.y);

            if (diff.x < size * 0.5f && diff.y < size * 0.5f && size > minSize_)
            {
                for (u32 j = 0; j < 4; j++)
                {
                    Node& child = nodes_[1 + j];
                    Math::Vec2 childCenter = center + offsets[j] * size * 0.25f;
                    child.posSize = Math::Vec4(childCenter, size * 0.5f, 0.0f);
                }
                nodeCount_ += 4;
                continue;
            }
        }

        for (u32 j = 0; j < 4; j++)
        {
            Node& node = nodes_[(1 + 4 * (i - 1)) + j];
            Math::Vec2 center = Math::Vec2(node.posSize);
            f32 size = node.posSize.z;

            Math::Vec2 diff = position - center;
            diff.x = Math::Abs(diff.x);
            diff.y = Math::Abs(diff.y);

            if (diff.x < size * 0.5f && diff.y < size * 0.5f && size > minSize_)
            {

                for (u32 k = 0; k < 4; k++)
                {
                    Node& child = nodes_[1 + 4 * i + k];
                    Math::Vec2 childCenter = center + offsets[k] * size * 0.25f;
                    child.posSize = Math::Vec4(childCenter, size * 0.5f, 0.0f);
                }
                nodeCount_ += 4;
                break;
            }
            FLY_LOG("%u", j);
        }
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateStorageBuffer(device, false, nodes_,
                                      sizeof(QuadTree::Node) * nodeCount_,
                                      quadBuffers[i]))
        {
            return false;
        }
    }

    return true;
}

void QuadTree::Destroy(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, quadBuffers[i]);
    }
}

void QuadTree::Update(Arena& arena, Math::Vec2 position) {}

} // namespace Fly
