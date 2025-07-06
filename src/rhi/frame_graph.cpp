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

    FrameGraph::ResourceDescriptor rd;
    rd.data = nullptr;
    rd.dataSize = 0;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.isExternal = true;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);

    builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    return builder.resourceCount++;
}

u32 CreateBufferDescriptor(Arena& arena, FrameGraph::Builder& builder,
                           VkBufferUsageFlags usage, bool hostVisible,
                           void* data, u64 dataSize,
                           FrameGraph::ResourceAccess accessMask)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Buffer;
    rd.isExternal = false;
    rd.buffer.usage = usage;
    rd.buffer.hostVisible = hostVisible;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);

    if (accessMask == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, builder.resourceCount);
    }
    else if (accessMask == FrameGraph::ResourceAccess::Write)
    {
        builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    }
    else
    {
        builder.currentPass->inputs.InsertBack(arena, builder.resourceCount);
        builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    }

    return builder.resourceCount++;
}

u32 CreateTextureDescriptor(Arena& arena, FrameGraph::Builder& builder,
                            VkImageUsageFlags usage, void* data, u64 dataSize,
                            u32 width, u32 height, VkFormat format,
                            Sampler::FilterMode filterMode,
                            Sampler::WrapMode wrapMode,
                            FrameGraph::ResourceAccess accessMask)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.isExternal = false;
    rd.texture2D.usage = usage;
    rd.texture2D.width = width;
    rd.texture2D.height = height;
    rd.texture2D.format = format;
    rd.texture2D.wrapMode = wrapMode;
    rd.texture2D.filterMode = filterMode;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);

    if (accessMask == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, builder.resourceCount);
    }
    else if (accessMask == FrameGraph::ResourceAccess::Write)
    {
        builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    }
    else
    {
        builder.currentPass->inputs.InsertBack(arena, builder.resourceCount);
        builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    }

    return builder.resourceCount++;
}

u32 CreateReference(Arena& arena, FrameGraph::Builder& builder,
                    FrameGraph::ResourceHandle rh,
                    FrameGraph::ResourceAccess accessMask)
{
    FrameGraph::ResourceDescriptor* rd = builder.resourceDescriptors.Find(rh);
    FLY_ASSERT(rd);

    if (accessMask == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, rh);
        return rh;
    }

    FrameGraph::ResourceDescriptor rdRef;
    rdRef.data = nullptr;
    rdRef.dataSize = 0;
    rdRef.type = FrameGraph::ResourceType::Reference;
    rdRef.isExternal = false;
    rdRef.reference.rd = rd;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rdRef);

    builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    if (accessMask == FrameGraph::ResourceAccess::ReadWrite)
    {
        builder.currentPass->inputs.InsertBack(arena, rh);
    }

    return builder.resourceCount++;
}

bool FrameGraph::Build(Arena& arena, RHI::Device& device)
{
    FrameGraph::Builder builder;

    for (PassNode* pass : passes_)
    {
        builder.currentPass = pass;
        pass->buildCallbackImpl(arena, builder, *pass);
    }

    for (PassNode* pass : passes_)
    {
        for (ResourceHandle rhI : pass->inputs)
        {
            for (PassNode* otherPass : passes_)
            {
                for (ResourceHandle rhO : otherPass->outputs)
                {
                    if (rhI == rhO)
                    {
                        otherPass->edges.InsertBack(arena, pass);
                    }
                }
            }
        }
    }

    for (PassNode* pass : passes_)
    {
        FLY_LOG("Pass name %s", pass->name);
        for (ResourceHandle rh : pass->inputs)
        {
            const FrameGraph::ResourceDescriptor* rd =
                builder.resourceDescriptors.Find(rh);
            FLY_ASSERT(rd);
            FLY_LOG("Input resource %u type %u, external: %d", rh, rd->type,
                    rd->isExternal);
        }

        for (ResourceHandle rh : pass->outputs)
        {
            const FrameGraph::ResourceDescriptor* rd =
                builder.resourceDescriptors.Find(rh);
            FLY_ASSERT(rd);
            FLY_LOG("Output resource %u type %u, external: %d", rh, rd->type,
                    rd->isExternal);
        }

        for (PassNode* edge : pass->edges)
        {
            FLY_LOG("Pass has edge to %s", edge->name);
        }
    }

    return true;
}

void FrameGraph::Execute(RHI::Device& device) { return; }

} // namespace RHI
} // namespace Fly
