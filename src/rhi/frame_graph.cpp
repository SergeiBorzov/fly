#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

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
                           FrameGraph::ResourceAccess access)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Buffer;
    rd.access = access;
    rd.isExternal = false;
    rd.buffer.usage = usage;
    rd.buffer.hostVisible = hostVisible;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);

    if (access == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, builder.resourceCount);
    }
    else if (access == FrameGraph::ResourceAccess::Write)
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
                            FrameGraph::ResourceAccess access)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.access = access;
    rd.isExternal = false;
    rd.texture2D.usage = usage;
    rd.texture2D.width = width;
    rd.texture2D.height = height;
    rd.texture2D.format = format;
    rd.texture2D.wrapMode = wrapMode;
    rd.texture2D.filterMode = filterMode;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rd);

    if (access == FrameGraph::ResourceAccess::Read)
    {
        builder.currentPass->inputs.InsertBack(arena, builder.resourceCount);
    }
    else if (access == FrameGraph::ResourceAccess::Write)
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
                    FrameGraph::ResourceAccess access)
{
    FrameGraph::ResourceDescriptor* rd = builder.resourceDescriptors.Find(rh);
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
    rdRef.reference.rd = rd;
    builder.resourceDescriptors.Insert(arena, builder.resourceCount, rdRef);

    builder.currentPass->outputs.InsertBack(arena, builder.resourceCount);
    if (access == FrameGraph::ResourceAccess::ReadWrite)
    {
        builder.currentPass->inputs.InsertBack(arena, rh);
    }

    return builder.resourceCount++;
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

bool FrameGraph::Build(Arena& arena, RHI::Device& device)
{
    FrameGraph::Builder builder;

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

    // TODO: Loop over resources and create single copy for reads only and
    // frame-in-flight copies for writes
    for (const HashTrie<u32, ResourceDescriptor>::Node* node :
         builder.resourceDescriptors)
    {
        const ResourceDescriptor& rd = node->value;
        if (rd.access == FrameGraph::ResourceAccess::Write ||
            rd.access == FrameGraph::ResourceAccess::ReadWrite)
        {
            FLY_LOG("resource %u type %u has write", node->key, rd.type);
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
