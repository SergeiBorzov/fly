#include "core/assert.h"
#include "core/log.h"

#include "frame_graph.h"

namespace Fly
{
namespace RHI
{

u32 SwapchainTextureDescriptor(Arena& arena, FrameGraph::Builder& builder)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::Builder::ResourceDescriptor rd;
    rd.data = nullptr;
    rd.dataSize = 0;
    rd.type = FrameGraph::Builder::ResourceDescriptor::Type::Texture2D;
    rd.isExternal = true;
    rd.accessMask = FrameGraph::ResourceAccess::Write;

    FrameGraph::ResourceInfo* resourceInfo =
        FLY_PUSH_ARENA(arena, FrameGraph::ResourceInfo, 1);
    resourceInfo->id = builder.resourceCount;
    resourceInfo->accessMask = rd.accessMask;
    resourceInfo->next = builder.currentPass->resources;

    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);
    builder.currentPass->resources = resourceInfo;
    return builder.resourceCount++;
}

u32 CreateBufferDescriptor(Arena& arena, FrameGraph::Builder& builder,
                           VkBufferUsageFlags usage, bool hostVisible,
                           void* data, u64 dataSize,
                           FrameGraph::ResourceAccess accessMask)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::Builder::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::Builder::ResourceDescriptor::Type::Buffer;
    rd.isExternal = false;
    rd.accessMask = accessMask;

    FrameGraph::ResourceInfo* resourceInfo =
        FLY_PUSH_ARENA(arena, FrameGraph::ResourceInfo, 1);
    resourceInfo->id = builder.resourceCount;
    resourceInfo->accessMask = rd.accessMask;
    resourceInfo->next = builder.currentPass->resources;

    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);
    builder.currentPass->resources = resourceInfo;
    return builder.resourceCount++;
}

bool FrameGraph::Build(Arena& arena)
{
    PassNode* curr = head_;

    FrameGraph::Builder builder;

    u32 rootPassCount = 0;
    while (curr)
    {
        builder.currentPass = curr;
        curr->buildCallbackImpl(arena, builder, *curr);
        if (curr->isRootPass)
        {
            rootPassCount += 1;
        }
        else
        {
            ResourceInfo* currR = curr->resources;
            while (currR)
            {
                FrameGraph::Builder::ResourceDescriptor* rd =
                    builder.resourceDescriptors.Find(currR->id);
                FLY_ASSERT(rd);
                if (rd->isExternal)
                {
                    rootPassCount += 1;
                    break;
                }
                currR = currR->next;
            }
        }
        curr = curr->next;
    }

    FLY_LOG("Found %u root passes", rootPassCount);

    return true;
}

} // namespace RHI
} // namespace Fly
