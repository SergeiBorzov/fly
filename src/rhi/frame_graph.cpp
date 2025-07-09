#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "device.h"
#include "frame_graph.h"

namespace Fly
{
namespace RHI
{

u32 CreateBuffer(Arena& arena, FrameGraph::Builder& builder,
                 VkBufferUsageFlags usage, bool hostVisible, void* data,
                 u64 dataSize, FrameGraph::ResourceAccess access)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Buffer;
    rd.access = access;
    rd.isExternal = false;
    rd.arrayIndex = -1;
    rd.buffer.usage = usage;
    rd.buffer.hostVisible = hostVisible;
    u32 id = static_cast<u32>(builder.frameGraph.resources.Count());
    builder.frameGraph.resources.Insert(arena, id, rd);

    if (access == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, id);
    }
    else if (access == FrameGraph::ResourceAccess::Write)
    {
        builder.currentPass->outputs.InsertBack(arena, id);
    }
    else
    {
        builder.currentPass->inputs.InsertBack(arena, id);
        builder.currentPass->outputs.InsertBack(arena, id);
    }

    return id;
}

u32 CreateTexture2D(Arena& arena, FrameGraph::Builder& builder,
                    VkImageUsageFlags usage, void* data, u64 dataSize,
                    u32 width, u32 height, VkFormat format,
                    Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                    FrameGraph::ResourceAccess access)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.access = access;
    rd.isExternal = false;
    rd.arrayIndex = -1;
    rd.texture2D.usage = usage;
    rd.texture2D.width = width;
    rd.texture2D.height = height;
    rd.texture2D.format = format;
    rd.texture2D.wrapMode = wrapMode;
    rd.texture2D.filterMode = filterMode;
    u32 id = static_cast<u32>(builder.frameGraph.resources.Count());
    builder.frameGraph.resources.Insert(arena, id, rd);

    if (access == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, id);
    }
    else if (access == FrameGraph::ResourceAccess::Write)
    {
        builder.currentPass->outputs.InsertBack(arena, id);
    }
    else
    {
        builder.currentPass->inputs.InsertBack(arena, id);
        builder.currentPass->outputs.InsertBack(arena, id);
    }

    return id;
}

u32 CreateReference(Arena& arena, FrameGraph::Builder& builder,
                    FrameGraph::ResourceHandle rh,
                    FrameGraph::ResourceAccess access)
{
    FrameGraph::ResourceDescriptor* rd = builder.frameGraph.resources.Find(rh);
    FLY_ASSERT(rd);

    if (access == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, rh);
        return rh;
    }

    FrameGraph::ResourceDescriptor rdRef;
    rdRef.data = nullptr;
    rdRef.dataSize = 0;
    rdRef.type = FrameGraph::ResourceType::Reference;
    rdRef.isExternal = false;
    rdRef.access = access;
    rdRef.arrayIndex = -1;
    rdRef.reference.rd = rd;
    u32 id = static_cast<u32>(builder.frameGraph.resources.Count());
    builder.frameGraph.resources.Insert(arena, id, rdRef);

    builder.currentPass->outputs.InsertBack(arena, id);
    if (access == FrameGraph::ResourceAccess::ReadWrite)
    {
        builder.currentPass->inputs.InsertBack(arena, rh);
    }

    return id;
}

// Tarjan topological sort
// static FrameGraph::PassNode**
// TopologicalSort(Arena& arena, const List<FrameGraph::PassNode*>& passes,
//                 u32& sortedCount)
// {
//     if (passes.Count() == 0)
//     {
//         return nullptr;
//     }

//     HashTrie<FrameGraph::PassNode*, u8> visited;
//     List<FrameGraph::PassNode*> stack;
//     for (FrameGraph::PassNode* pass : passes)
//     {
//         visited.Insert(arena, pass, 0);
//         stack.InsertFront(arena, pass);
//     }

//     FrameGraph::PassNode** sorted =
//         FLY_PUSH_ARENA(arena, FrameGraph::PassNode*, passes.Count());
//     sortedCount = 0;

//     while (stack.Count())
//     {
//         FrameGraph::PassNode* head = *(stack.Head());

//         u8 value = *(visited.Find(head));

//         if (value == 0)
//         {
//             visited.Insert(arena, head, 1);
//             for (FrameGraph::PassNode* child : head->edges)
//             {
//                 u8* pValue = visited.Find(child);
//                 if ((*pValue) == 0)
//                 {
//                     stack.InsertFront(arena, child);
//                     visited.Insert(arena, child, 1);
//                 }
//                 else if ((*pValue) == 1)
//                 {
//                     FLY_ASSERT(false, "Cycle detected!");
//                     return nullptr;
//                 }
//             }
//             continue;
//         }
//         else if (value == 1)
//         {
//             visited.Insert(arena, head, 2);
//             sorted[sortedCount++] = head;
//             stack.PopFront();
//         }
//         else
//         {
//             stack.PopFront();
//         }
//     }

//     return sorted;
// }

bool FrameGraph::Build(Arena& arena)
{
    FrameGraph::Builder builder(*this);

    // Fill resource descriptors
    for (PassNode* pass : passes_)
    {
        builder.currentPass = pass;
        pass->buildCallbackImpl(arena, builder, *pass);
    }

    // Create edges
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

    // Passes are already sorted in topological order
    // Otherwise the build function will assert trying to reference non-existing
    // resource
    // TODO: cycles are not handled though

    bufferCount_ = 0;
    textureCount_ = 0;

    for (const HashTrie<u32, ResourceDescriptor>::Node* node : resources)
    {
        const ResourceDescriptor& rd = node->value;

        // Note for now all commands are submitted to one graphics/compute queue
        // So replication is only required for swapchain color attachment
        // And buffers that will be updated from CPU each frame (hostVisible
        // uniform/storage) Logic here might change if async compute will be
        // introduced
        if (rd.type == FrameGraph::ResourceType::Buffer)
        {
            u32 count = rd.buffer.hostVisible ? FLY_FRAME_IN_FLIGHT_COUNT : 1;
            bufferCount_ += count;
        }
        else if (rd.type == FrameGraph::ResourceType::Texture2D)
        {
            textureCount_++;
        }
        else if (rd.type == FrameGraph::ResourceType::Reference)
        {
            FLY_ASSERT(rd.reference.rd);
            if (rd.access != FrameGraph::ResourceAccess::Read)
            {
                FLY_ASSERT(rd.reference.rd->access !=
                               FrameGraph::ResourceAccess::Read,
                           "Write after read use-case? Probably error");
            }
        }
    }

    if (bufferCount_ > 0)
    {
        buffers_ = FLY_PUSH_ARENA(arena, RHI::Buffer, bufferCount_);
    }
    if (textureCount_ > 0)
    {
        textures_ = FLY_PUSH_ARENA(arena, RHI::Texture2D, textureCount_);
    }

    u32 bufferIndex = 0;
    u32 textureIndex = 0;
    for (HashTrie<u32, ResourceDescriptor>::Node* node : resources)
    {
        ResourceDescriptor& rd = node->value;

        if (rd.type == FrameGraph::ResourceType::Buffer)
        {
            u32 count = rd.buffer.hostVisible ? FLY_FRAME_IN_FLIGHT_COUNT : 1;

            for (u32 i = 0; i < count; i++)
            {
                bool res = RHI::CreateBuffer(
                    device_, rd.buffer.hostVisible, rd.buffer.usage, rd.data,
                    rd.dataSize, buffers_[bufferIndex]);
                FLY_ASSERT(res);
                rd.arrayIndex = bufferIndex++;
            }
        }
        else if (rd.type == FrameGraph::ResourceType::Texture2D)
        {
            bool res = RHI::CreateTexture2D(
                device_, rd.texture2D.usage, rd.data, rd.dataSize,
                rd.texture2D.width, rd.texture2D.height, rd.texture2D.format,
                rd.texture2D.filterMode, rd.texture2D.wrapMode,
                textures_[textureIndex]);
            FLY_ASSERT(res);
            rd.arrayIndex = textureIndex++;
        }
    }

    for (HashTrie<u32, ResourceDescriptor>::Node* node : resources)
    {
        ResourceDescriptor& rd = node->value;
        if (rd.type == FrameGraph::ResourceType::Reference)
        {
            rd.arrayIndex = rd.reference.rd->arrayIndex;
        }
    }

    for (PassNode* pass : passes_)
    {
        FLY_LOG("Pass name %s", pass->name);
        for (ResourceHandle rh : pass->inputs)
        {
            const FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
            FLY_ASSERT(rd);
            FLY_LOG("Input resource %u type %u, index - %d, external: %d", rh,
                    rd->type, rd->arrayIndex, rd->isExternal);
        }

        for (ResourceHandle rh : pass->outputs)
        {
            const FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
            FLY_ASSERT(rd);
            FLY_LOG("Output resource %u type %u, index - %d, external: %d", rh,
                    rd->type, rd->arrayIndex, rd->isExternal);
        }

        for (PassNode* edge : pass->edges)
        {
            FLY_LOG("Pass has edge to %s", edge->name);
        }
    }

    return true;
}

void FrameGraph::Destroy()
{
    for (u32 i = 0; i < bufferCount_; i++)
    {
        RHI::DestroyBuffer(device_, buffers_[i]);
    }

    for (u32 i = 0; i < textureCount_; i++)
    {
        RHI::DestroyTexture2D(device_, textures_[i]);
    }
}

void FrameGraph::Execute()
{
    // TODO: Memory barriers!
    for (PassNode* pass : passes_)
    {
        pass->recordCallbackImpl(RenderFrameCommandBuffer(device_), *pass);
    }
}

} // namespace RHI
} // namespace Fly
