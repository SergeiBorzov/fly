#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "device.h"
#include "frame_graph.h"

static VkRenderingAttachmentInfo
ColorAttachmentInfo(VkImageView imageView, VkAttachmentLoadOp loadOp,
                    VkAttachmentStoreOp storeOp, VkClearColorValue clearColor)
{
    VkRenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.pNext = nullptr;
    attachmentInfo.imageView = imageView;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp = loadOp;
    attachmentInfo.storeOp = storeOp;
    attachmentInfo.clearValue.color = clearColor;

    return attachmentInfo;
}

static VkRenderingAttachmentInfo
DepthAttachmentInfo(VkImageView imageView, VkAttachmentLoadOp loadOp,
                    VkAttachmentStoreOp storeOp,
                    VkClearDepthStencilValue clearDepthStencil)
{
    VkRenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.pNext = nullptr;
    attachmentInfo.imageView = imageView;
    attachmentInfo.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp = loadOp;
    attachmentInfo.storeOp = storeOp;
    attachmentInfo.clearValue.depthStencil = clearDepthStencil;

    return attachmentInfo;
}

static VkRenderingInfo
RenderingInfo(const VkRect2D& renderArea,
              const VkRenderingAttachmentInfo* colorAttachments,
              u32 colorAttachmentCount,
              const VkRenderingAttachmentInfo* depthAttachment,
              const VkRenderingAttachmentInfo* stencilAttachment,
              u32 layerCount, u32 viewMask)
{
    FLY_ASSERT(colorAttachments);
    FLY_ASSERT(colorAttachmentCount > 0);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = 0;
    renderInfo.viewMask = viewMask;
    renderInfo.layerCount = layerCount;
    renderInfo.renderArea = renderArea;
    renderInfo.colorAttachmentCount = colorAttachmentCount;
    renderInfo.pColorAttachments = colorAttachments;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = stencilAttachment;

    return renderInfo;
}

namespace Fly
{
namespace RHI
{

struct AttachmentCount
{
    u32 color;
    u32 depth;
};

static AttachmentCount CountAttachments(
    const FrameGraph::PassNode* pass,
    const HashTrie<ResourceHandle, FrameGraph::ResourceDescriptor>& resources)
{
    FLY_ASSERT(pass);

    AttachmentCount count{};
    for (ResourceHandle rh : pass->outputs)
    {
        const FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
        FLY_ASSERT(rd);

        if (rd->type == FrameGraph::ResourceType::Texture2D)
        {
            if (rd->texture2D.usage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            {
                count.color++;
            }
            else if (rd->texture2D.usage ==
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                count.depth++;
            }
        }
    }

    return count;
}

const FrameGraph::TextureHandle FrameGraph::TextureHandle::sBackBuffer = {
    {0, 0}};

static ResourceHandle GetNextHandle(
    const HashTrie<ResourceHandle, FrameGraph::ResourceDescriptor>& resources)
{
    ResourceHandle handle;
    handle.id = static_cast<u32>(resources.Count()) + 1;
    handle.version = 0;
    return handle;
}

FrameGraph::BufferHandle CreateBuffer(Arena& arena,
                                      FrameGraph::Builder& builder,
                                      VkBufferUsageFlags usage,
                                      bool hostVisible, void* data,
                                      u64 dataSize)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Buffer;
    rd.access = FrameGraph::ResourceAccess::Unknown;
    rd.isExternal = false;
    rd.arrayIndex = -1;
    rd.buffer.usage = usage;
    rd.buffer.hostVisible = hostVisible;
    ResourceHandle handle = GetNextHandle(builder.frameGraph.resources);
    builder.frameGraph.resources.Insert(arena, handle, rd);

    return {handle};
}

FrameGraph::TextureHandle
CreateTexture2D(Arena& arena, FrameGraph::Builder& builder,
                VkImageUsageFlags usage, void* data, u64 dataSize, u32 width,
                u32 height, VkFormat format, Sampler::FilterMode filterMode,
                Sampler::WrapMode wrapMode)
{
    FLY_ASSERT(builder.currentPass);

    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.dataSize = dataSize;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.access = FrameGraph::ResourceAccess::Unknown;
    rd.isExternal = false;
    rd.arrayIndex = -1;
    rd.texture2D.usage = usage;
    rd.texture2D.width = width;
    rd.texture2D.height = height;
    rd.texture2D.format = format;
    rd.texture2D.wrapMode = wrapMode;
    rd.texture2D.filterMode = filterMode;
    ResourceHandle handle = GetNextHandle(builder.frameGraph.resources);
    builder.frameGraph.resources.Insert(arena, handle, rd);

    return {handle};
}

FrameGraph::TextureHandle
ColorAttachment(Arena& arena, FrameGraph::Builder& builder, u32 index,
                FrameGraph::TextureHandle textureHandle,
                VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                VkClearColorValue clearColor)
{
    FLY_ASSERT(builder.currentPass);
    FLY_ASSERT(builder.currentPass->type ==
               FrameGraph::PassNode::Type::Graphics);

    if (textureHandle == FrameGraph::TextureHandle::sBackBuffer)
    {
        FrameGraph::ResourceDescriptor rd{};
        rd.type = FrameGraph::ResourceType::Texture2D;
        rd.texture2D.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        ResourceHandle rh;
        rh.id = FLY_SWAPCHAIN_TEXTURE_HANDLE_ID;
        rh.version = 0;
        builder.frameGraph.resources.Insert(arena, rh, rd);
    }

    FrameGraph::ResourceDescriptor* rd =
        builder.frameGraph.resources.Find(textureHandle.handle);
    FLY_ASSERT(rd);

    rd->texture2D.index = index;
    rd->texture2D.loadOp = loadOp;
    rd->texture2D.storeOp = storeOp;
    rd->texture2D.clearValue.color = clearColor;
    builder.currentPass->outputs.InsertFront(arena, textureHandle.handle);

    ResourceHandle rh;
    rh.id = textureHandle.handle.id;
    rh.version = textureHandle.handle.version + 1;
    builder.frameGraph.resources.Insert(arena, rh, *rd);

    return {rh};
}

// FrameGraph::TextureHandle
// ColorAttachment(u32 index, Fly::TextureHandle textureHandle,
//                 VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}},
//                 VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
//                 VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE)
// {
//     FrameGraph::ResourceDescriptor* rd =
//         builder.frameGraph.resources.Find(textureHandle.id);

//     if (textureHandle.id == FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
//     {
//         FrameGraph::ResourceDescriptor backBufferRD;
//         if (!rd)
//         {
//             backBufferRD.access = FrameGraph::ResourceAccess::Write;
//             backBufferRD.texture2D.index = index;
//             backBufferRD.texture2D.loadOp = loadOp;
//             backBufferRD.texture2D.storeOp = storeOp;
//             backBufferRD.isExternal = true;
//             backBufferRD.data = nullptr;
//             backBufferRD.dataSize = 0;
//             backBufferRD.arrayIndex = -1;
//             builder.frameGraph.Resources.Insert(arena, 0, rd);
//         }
//         return {0};
//     }
//     else
//     {
//     }
// }

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
        // write -> reads
        for (ResourceHandle inputHandle : pass->inputs)
        {
            for (PassNode* otherPass : passes_)
            {
                if (otherPass == pass)
                {
                    continue;
                }

                for (ResourceHandle outputHandle : otherPass->outputs)
                {
                    if (inputHandle == outputHandle)
                    {
                        otherPass->edges.InsertBack(arena, pass);
                    }
                }
            }
        }

        // write -> writes
        for (ResourceHandle outputHandle : pass->outputs)
        {
            if (outputHandle.version == 0)
            {
                continue;
            }

            for (PassNode* otherPass : passes_)
            {
                if (otherPass == pass)
                {
                    continue;
                }
                for (ResourceHandle otherOutputHandle : otherPass->outputs)
                {
                    if (outputHandle.id == otherOutputHandle.id &&
                        outputHandle.version - 1 == otherOutputHandle.version)
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

    for (const HashTrie<ResourceHandle, ResourceDescriptor>::Node* node :
         resources)
    {
        ResourceHandle handle = node->key;
        const ResourceDescriptor& rd = node->value;

        // Note for now all commands are submitted to one graphics/compute queue
        // So replication is only required for swapchain color attachment
        // And buffers that will be updated from CPU each frame (hostVisible
        // uniform/storage) Logic here might change if async compute will be
        // introduced

        if (rd.type == FrameGraph::ResourceType::Buffer && handle.version == 0)
        {
            u32 count = rd.buffer.hostVisible ? FLY_FRAME_IN_FLIGHT_COUNT : 1;
            bufferCount_ += count;
        }
        else if (rd.type == FrameGraph::ResourceType::Texture2D &&
                 handle.version == 0 &&
                 handle.id != FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
        {
            textureCount_++;
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
    for (HashTrie<ResourceHandle, ResourceDescriptor>::Node* node : resources)
    {
        ResourceHandle handle = node->key;
        ResourceDescriptor& rd = node->value;

        if (rd.type == FrameGraph::ResourceType::Buffer && handle.version == 0)
        {
            u32 count = rd.buffer.hostVisible ? FLY_FRAME_IN_FLIGHT_COUNT : 1;
            u32 arrayIndex = bufferIndex;
            for (u32 i = 0; i < count; i++)
            {
                bool res = RHI::CreateBuffer(
                    device_, rd.buffer.hostVisible, rd.buffer.usage, rd.data,
                    rd.dataSize, buffers_[bufferIndex++]);
                FLY_ASSERT(res);
            }
            rd.arrayIndex = arrayIndex;
        }
        else if (rd.type == FrameGraph::ResourceType::Texture2D &&
                 handle.version == 0 &&
                 handle.id != FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
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

    for (HashTrie<ResourceHandle, ResourceDescriptor>::Node* node : resources)
    {
        ResourceHandle handle = node->key;
        ResourceDescriptor& rd = node->value;

        if (handle.version != 0)
        {
            ResourceHandle rootHandle = {handle.id, 0};
            const FrameGraph::ResourceDescriptor* rootRD =
                resources.Find(rootHandle);
            FLY_ASSERT(rootRD);
            rd.arrayIndex = rootRD->arrayIndex;
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
            FLY_LOG(
                "Output resource (%u, %u) type %u, index - %d, external: %d",
                rh.id, rh.version, rd->type, rd->arrayIndex, rd->isExternal);
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
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::BeginRenderFrame(device_);

    // TODO: Memory barriers, image layout transitions, compute pass

    for (PassNode* pass : passes_)
    {
        if (pass->type == PassNode::Type::Graphics)
        {

            AttachmentCount count = CountAttachments(pass, resources);
            FLY_ASSERT(count.color);
            FLY_ASSERT(count.depth <= 1);

            VkRenderingAttachmentInfo* attachments = FLY_PUSH_ARENA(
                arena, VkRenderingAttachmentInfo, count.color + count.depth);

            u32 i = 0;
            VkRect2D renderArea;
            renderArea.offset = {0, 0};
            for (ResourceHandle rh : pass->outputs)
            {
                const FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
                FLY_ASSERT(rd);

                if (rd->type == FrameGraph::ResourceType::Texture2D)
                {
                    VkImageView imageView;
                    if (rh.id == FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
                    {
                        const SwapchainTexture& swapchainTexture =
                            RenderFrameSwapchainTexture(device_);
                        imageView = swapchainTexture.imageView;
                        renderArea.extent = {swapchainTexture.width,
                                             swapchainTexture.height};
                    }
                    else
                    {
                        const RHI::Texture2D& texture =
                            textures_[rd->arrayIndex];
                        imageView = texture.imageView;
                        renderArea.extent = {texture.width, texture.height};
                    }

                    if (rd->texture2D.usage ==
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                    {
                        attachments[i++] =
                            ColorAttachmentInfo(imageView, rd->texture2D.loadOp,
                                                rd->texture2D.storeOp,
                                                rd->texture2D.clearValue.color);
                    }
                    else if (rd->texture2D.usage ==
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                    {
                        attachments[count.color] = DepthAttachmentInfo(
                            imageView, rd->texture2D.loadOp,
                            rd->texture2D.storeOp,
                            rd->texture2D.clearValue.depthStencil);
                    }
                }
            }

            VkRenderingAttachmentInfo* pDepthAttachment =
                (count.depth == 0) ? nullptr : attachments + count.color;
            VkRenderingInfo renderInfo =
                RenderingInfo(renderArea, attachments, count.color,
                              pDepthAttachment, nullptr, 1, 0);

            RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device_);
            vkCmdBeginRendering(cmd.handle, &renderInfo);
            pass->recordCallbackImpl(cmd, *pass);
            vkCmdEndRendering(cmd.handle);
        }
    }

    RHI::EndRenderFrame(device_);
    ArenaPopToMarker(arena, marker);
}

} // namespace RHI
} // namespace Fly
