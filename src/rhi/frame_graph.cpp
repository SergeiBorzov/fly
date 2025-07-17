#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "context.h"
#include "device.h"
#include "frame_graph.h"

static void OnSwapchainRecreated(u32 w, u32 h, void* data)
{
    Fly::RHI::FrameGraph* fg = static_cast<Fly::RHI::FrameGraph*>(data);
    fg->ResizeDynamicTextures();
}

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

static ResourceHandle CommonRead(Arena& arena, ResourceHandle rh,
                                 const FrameGraph::ResourceMap& resources,
                                 List<ResourceHandle>& inputs,
                                 const List<ResourceHandle>& outputs)
{
#ifndef NDEBUG
    for (ResourceHandle handle : inputs)
    {
        FLY_ASSERT(handle.id != rh.id);
    }

    for (ResourceHandle handle : outputs)
    {
        FLY_ASSERT(handle.id != rh.id);
    }
#endif

    const FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
    FLY_ASSERT(rd);

    inputs.InsertFront(arena, rh);
    return rh;
}

static ResourceHandle CommonWrite(Arena& arena, ResourceHandle rh,
                                  FrameGraph::ResourceMap& resources,
                                  const List<ResourceHandle>& inputs,
                                  List<ResourceHandle>& outputs)
{
#ifndef NDEBUG
    for (ResourceHandle handle : inputs)
    {
        FLY_ASSERT(handle.id != rh.id);
    }

    for (ResourceHandle handle : outputs)
    {
        FLY_ASSERT(handle.id != rh.id);
    }
#endif

    FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
    FLY_ASSERT(rd);

    ResourceHandle wh;
    wh.id = rh.id;
    wh.version = rh.version + 1;
    FLY_ASSERT(resources.Find(wh) == nullptr);
    resources.Insert(arena, wh, *rd);

    outputs.InsertFront(arena, wh);
    return {wh};
}

static ResourceHandle CommonReadWrite(Arena& arena, ResourceHandle rh,
                                      FrameGraph::ResourceMap& resources,
                                      List<ResourceHandle>& inputs,
                                      List<ResourceHandle>& outputs)
{
#ifndef NDEBUG
    for (ResourceHandle handle : inputs)
    {
        FLY_ASSERT(handle.id != rh.id);
    }

    for (ResourceHandle handle : outputs)
    {
        FLY_ASSERT(handle.id != rh.id);
    }
#endif

    FrameGraph::ResourceDescriptor* rd = resources.Find(rh);
    FLY_ASSERT(rd);

    ResourceHandle wh;
    wh.id = rh.id;
    wh.version = rh.version + 1;
    FLY_ASSERT(resources.Find(wh) == nullptr);
    resources.Insert(arena, wh, *rd);

    inputs.InsertFront(arena, rh);
    outputs.InsertFront(arena, wh);
    return {wh};
}

const FrameGraph::TextureHandle FrameGraph::TextureHandle::sBackBuffer = {
    {0, 0}};

struct AttachmentCount
{
    u32 color;
    u32 depth;
};

static AttachmentCount
CountAttachments(const FrameGraph::PassNode* pass,
                 const FrameGraph::ResourceMap& resources)
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

const RHI::Buffer&
FrameGraph::ResourceMap::GetBuffer(BufferHandle bufferHandle) const
{
    const ResourceDescriptor* rd = resources_.Find(bufferHandle.handle);
    FLY_ASSERT(rd);

    u32 offset =
        (!rd->buffer.hostVisible) ? 0 : frameGraph_->GetSwapchainIndex();

    return frameGraph_->buffers_[rd->arrayIndex + offset];
}

const RHI::Texture2D&
FrameGraph::ResourceMap::GetTexture2D(TextureHandle textureHandle) const
{
    const ResourceDescriptor* rd = resources_.Find(textureHandle.handle);
    FLY_ASSERT(rd);
    return frameGraph_->textures_[rd->arrayIndex];
}

RHI::Buffer& FrameGraph::ResourceMap::GetBuffer(BufferHandle bufferHandle)
{
    const ResourceDescriptor* rd = resources_.Find(bufferHandle.handle);
    FLY_ASSERT(rd);

    u32 offset =
        (!rd->buffer.hostVisible) ? 0 : frameGraph_->GetSwapchainIndex();

    return frameGraph_->buffers_[rd->arrayIndex + offset];
}

RHI::Texture2D&
FrameGraph::ResourceMap::GetTexture2D(TextureHandle textureHandle)
{
    const ResourceDescriptor* rd = resources_.Find(textureHandle.handle);
    FLY_ASSERT(rd);
    return frameGraph_->textures_[rd->arrayIndex];
}

ResourceHandle FrameGraph::ResourceMap::GetNextHandle()

{
    return {++nextHandle_, 0};
}

FrameGraph::BufferHandle
FrameGraph::Builder::CreateBuffer(Arena& arena, VkBufferUsageFlags usage,
                                  bool hostVisible, const void* data, u64 dataSize)
{
    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.type = FrameGraph::ResourceType::Buffer;
    rd.arrayIndex = -1;
    rd.buffer.size = dataSize;
    rd.buffer.usage = usage;
    rd.buffer.hostVisible = hostVisible;
    rd.buffer.external = nullptr;

    ResourceHandle rh = resources_.GetNextHandle();
    FLY_ASSERT(resources_.Find(rh) == nullptr);
    resources_.Insert(arena, rh, rd);

    return {rh};
}

FrameGraph::TextureHandle FrameGraph::Builder::CreateTexture2D(
    Arena& arena, VkImageUsageFlags usage, const void* data, u32 width, u32 height,
    VkFormat format, Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode)
{
    FrameGraph::ResourceDescriptor rd;
    rd.data = data;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.arrayIndex = -1;
    rd.texture2D.sizeType = FrameGraph::TextureSizeType::Fixed;
    rd.texture2D.usage = usage;
    rd.texture2D.width = width;
    rd.texture2D.height = height;
    rd.texture2D.format = format;
    rd.texture2D.wrapMode = wrapMode;
    rd.texture2D.filterMode = filterMode;
    rd.texture2D.external = nullptr;

    ResourceHandle rh = resources_.GetNextHandle();
    FLY_ASSERT(resources_.Find(rh) == nullptr);
    resources_.Insert(arena, rh, rd);

    return {rh};
}

FrameGraph::TextureHandle
FrameGraph::Builder::CreateTexture2D(Arena& arena, VkImageUsageFlags usage,
                                     f32 relativeSizeX, f32 relativeSizeY,
                                     VkFormat format)
{
    FrameGraph::ResourceDescriptor rd;
    rd.data = nullptr;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.arrayIndex = -1;
    rd.texture2D.sizeType = FrameGraph::TextureSizeType::SwapchainRelative;
    rd.texture2D.usage = usage;
    rd.texture2D.relativeSize.x = relativeSizeX;
    rd.texture2D.relativeSize.y = relativeSizeY;
    rd.texture2D.format = format;
    rd.texture2D.external = nullptr;

    ResourceHandle rh = resources_.GetNextHandle();
    FLY_ASSERT(resources_.Find(rh) == nullptr);
    resources_.Insert(arena, rh, rd);

    return {rh};
}

FrameGraph::TextureHandle FrameGraph::Builder::ColorAttachment(
    Arena& arena, u32 index, FrameGraph::TextureHandle textureHandle,
    VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
    VkClearColorValue clearColor)
{
    FLY_ASSERT(currentPass_);
    FLY_ASSERT(currentPass_->type == FrameGraph::PassNode::Type::Graphics);

    if (textureHandle == FrameGraph::TextureHandle::sBackBuffer &&
        !resources_.Find(FrameGraph::TextureHandle::sBackBuffer.handle))
    {
        FrameGraph::ResourceDescriptor rd{};
        rd.type = FrameGraph::ResourceType::Texture2D;
        rd.texture2D.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        ResourceHandle rh;
        rh.id = FLY_SWAPCHAIN_TEXTURE_HANDLE_ID;
        rh.version = 0;

        resources_.Insert(arena, rh, rd);
    }

    FrameGraph::ResourceDescriptor* rd = resources_.Find(textureHandle.handle);
    FLY_ASSERT(rd);

    rd->texture2D.index = index;
    rd->texture2D.loadOp = loadOp;
    rd->texture2D.storeOp = storeOp;
    rd->texture2D.clearValue.color = clearColor;
    currentPass_->outputs.InsertFront(arena, textureHandle.handle);

    ResourceHandle rh;
    rh.id = textureHandle.handle.id;
    rh.version = textureHandle.handle.version + 1;
    FLY_ASSERT(resources_.Find(rh) == nullptr);
    resources_.Insert(arena, rh, *rd);

    return {rh};
}

FrameGraph::BufferHandle
FrameGraph::Builder::Read(Arena& arena, FrameGraph::BufferHandle bufferHandle)
{
    FLY_ASSERT(currentPass_);

    ResourceHandle rh = CommonRead(arena, bufferHandle.handle, resources_,
                                   currentPass_->inputs, currentPass_->outputs);
    return {rh};
}

FrameGraph::TextureHandle
FrameGraph::Builder::Read(Arena& arena, FrameGraph::TextureHandle textureHandle)
{
    FLY_ASSERT(currentPass_);

    ResourceHandle rh = CommonRead(arena, textureHandle.handle, resources_,
                                   currentPass_->inputs, currentPass_->outputs);
    return {rh};
}

FrameGraph::BufferHandle
FrameGraph::Builder::Write(Arena& arena, FrameGraph::BufferHandle bufferHandle)
{
    FLY_ASSERT(currentPass_);

    ResourceHandle rh =
        CommonWrite(arena, bufferHandle.handle, resources_,
                    currentPass_->inputs, currentPass_->outputs);
    return {rh};
}

FrameGraph::TextureHandle
FrameGraph::Builder::Write(Arena& arena,
                           FrameGraph::TextureHandle textureHandle)
{
    FLY_ASSERT(currentPass_);

    ResourceHandle rh =
        CommonWrite(arena, textureHandle.handle, resources_,
                    currentPass_->inputs, currentPass_->outputs);
    return {rh};
}

FrameGraph::BufferHandle
FrameGraph::Builder::ReadWrite(Arena& arena,
                               FrameGraph::BufferHandle bufferHandle)
{
    FLY_ASSERT(currentPass_);

    ResourceHandle rh =
        CommonReadWrite(arena, bufferHandle.handle, resources_,
                        currentPass_->inputs, currentPass_->outputs);
    return {rh};
}

FrameGraph::TextureHandle
FrameGraph::Builder::ReadWrite(Arena& arena,
                               FrameGraph::TextureHandle textureHandle)
{
    FLY_ASSERT(currentPass_);

    ResourceHandle rh =
        CommonReadWrite(arena, textureHandle.handle, resources_,
                        currentPass_->inputs, currentPass_->outputs);
    return {rh};
}

FrameGraph::BufferHandle
FrameGraph::Builder::RegisterExternalBuffer(Arena& arena, RHI::Buffer& buffer)
{
    FrameGraph::ResourceDescriptor rd;
    rd.data = nullptr;
    rd.type = FrameGraph::ResourceType::Buffer;
    rd.arrayIndex = -1;
    rd.buffer.external = &buffer;
    rd.buffer.size = buffer.allocationInfo.size;
    rd.buffer.usage = buffer.usage;
    rd.buffer.hostVisible = buffer.hostVisible;

    ResourceHandle handle = resources_.GetNextHandle();
    resources_.Insert(arena, handle, rd);

    return {handle};
}

FrameGraph::TextureHandle
FrameGraph::Builder::RegisterExternalTexture2D(Arena& arena,
                                               RHI::Texture2D& texture2D)
{
    FrameGraph::ResourceDescriptor rd;
    rd.data = nullptr;
    rd.type = FrameGraph::ResourceType::Texture2D;
    rd.arrayIndex = -1;
    rd.texture2D.sizeType = FrameGraph::TextureSizeType::Fixed;
    rd.texture2D.usage = texture2D.usage;
    rd.texture2D.width = texture2D.width;
    rd.texture2D.height = texture2D.height;
    rd.texture2D.format = texture2D.format;
    rd.texture2D.wrapMode = texture2D.sampler.wrapMode;
    rd.texture2D.filterMode = texture2D.sampler.filterMode;
    rd.texture2D.external = &texture2D;

    ResourceHandle handle = resources_.GetNextHandle();
    resources_.Insert(arena, handle, rd);

    return {handle};
}

FrameGraph::TextureHandle FrameGraph::Builder::DepthAttachment(
    Arena& arena, FrameGraph::TextureHandle textureHandle,
    VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
    VkClearDepthStencilValue clearDepthStencil)
{
    FLY_ASSERT(currentPass_);
    FLY_ASSERT(currentPass_->type == FrameGraph::PassNode::Type::Graphics);
    FLY_ASSERT(textureHandle != FrameGraph::TextureHandle::sBackBuffer);

    FrameGraph::ResourceDescriptor* rd = resources_.Find(textureHandle.handle);
    FLY_ASSERT(rd);

    rd->texture2D.loadOp = loadOp;
    rd->texture2D.storeOp = storeOp;
    rd->texture2D.clearValue.depthStencil = clearDepthStencil;
    currentPass_->outputs.InsertFront(arena, textureHandle.handle);

    ResourceHandle rh;
    rh.id = textureHandle.handle.id;
    rh.version = textureHandle.handle.version + 1;
    resources_.Insert(arena, rh, *rd);

    return {rh};
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

FrameGraph::FrameGraph(RHI::Device& device) : resources_(this), device_(device)
{
}

bool FrameGraph::Build(Arena& arena)
{
    FrameGraph::Builder builder(resources_);

    // Fill resource descriptors
    for (PassNode* pass : passes_)
    {
        builder.currentPass_ = pass;
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
         resources_)
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

    for (HashTrie<ResourceHandle, ResourceDescriptor>::Node* node : resources_)
    {
        ResourceHandle handle = node->key;
        ResourceDescriptor& rd = node->value;

        if (rd.type == FrameGraph::ResourceType::Buffer && handle.version == 0)
        {
            if (!rd.buffer.external)
            {
                u32 count =
                    rd.buffer.hostVisible ? FLY_FRAME_IN_FLIGHT_COUNT : 1;
                u32 arrayIndex = bufferIndex;
                for (u32 i = 0; i < count; i++)
                {
                    bool res = RHI::CreateBuffer(
                        device_, rd.buffer.hostVisible, rd.buffer.usage,
                        rd.data, rd.buffer.size, buffers_[bufferIndex++]);
                    FLY_ASSERT(res);
                }
                rd.arrayIndex = arrayIndex;
            }
            else
            {
                buffers_[bufferIndex] = *rd.buffer.external;
                rd.arrayIndex = bufferIndex++;
            }
        }
        else if (rd.type == FrameGraph::ResourceType::Texture2D &&
                 handle.version == 0 &&
                 handle.id != FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
        {
            if (!rd.texture2D.external)
            {
                u32 width = rd.texture2D.width;
                u32 height = rd.texture2D.height;

                if (rd.texture2D.sizeType == TextureSizeType::SwapchainRelative)
                {
                    FLY_ASSERT(device_.context->windowPtr,
                               "No swapchain, cannot create dynamic texture");
                    width = static_cast<u32>(device_.swapchainWidth *
                                             rd.texture2D.relativeSize.x);
                    height = static_cast<u32>(device_.swapchainHeight *
                                              rd.texture2D.relativeSize.y);
                }

                bool res = RHI::CreateTexture2D(
                    device_, rd.texture2D.usage, rd.data, width, height,
                    rd.texture2D.format, rd.texture2D.filterMode,
                    rd.texture2D.wrapMode, textures_[textureIndex]);
                FLY_ASSERT(res);
                rd.arrayIndex = textureIndex++;
            }
            else
            {
                textures_[textureIndex] = *rd.texture2D.external;
                rd.arrayIndex = textureIndex++;
            }
        }
    }

    for (HashTrie<ResourceHandle, ResourceDescriptor>::Node* node : resources_)
    {
        ResourceHandle handle = node->key;
        ResourceDescriptor& rd = node->value;

        if (handle.version != 0)
        {
            ResourceHandle rootHandle = {handle.id, 0};
            const FrameGraph::ResourceDescriptor* rootRD =
                resources_.Find(rootHandle);
            FLY_ASSERT(rootRD);
            rd.arrayIndex = rootRD->arrayIndex;
        }
    }

    for (PassNode* pass : passes_)
    {
        FLY_LOG("Pass name %s", pass->name);
        for (ResourceHandle rh : pass->inputs)
        {
            const FrameGraph::ResourceDescriptor* rd = resources_.Find(rh);
            FLY_ASSERT(rd);
            FLY_LOG("Input resource (%u, %u) type %u, index - %d", rh.id,
                    rh.version, rd->type, rd->arrayIndex);
        }

        for (ResourceHandle rh : pass->outputs)
        {
            const FrameGraph::ResourceDescriptor* rd = resources_.Find(rh);
            FLY_ASSERT(rd);
            FLY_LOG("Output resource (%u, %u) type %u, index - %d", rh.id,
                    rh.version, rd->type, rd->arrayIndex);
        }

        for (PassNode* edge : pass->edges)
        {
            FLY_LOG("Pass has edge to %s", edge->name);
        }
    }

    device_.swapchainRecreatedCallbacks.InsertFront(
        arena, {OnSwapchainRecreated, this});

    return true;
}

void FrameGraph::Destroy()
{
    device_.swapchainRecreatedCallbacks.Remove({OnSwapchainRecreated, this});
    for (HashTrie<ResourceHandle, ResourceDescriptor>::Node* node : resources_)
    {
        ResourceHandle handle = node->key;
        ResourceDescriptor& rd = node->value;

        if (rd.type == ResourceType::Buffer)
        {
            if (rd.buffer.external || handle.version != 0)
            {
                continue;
            }

            u32 count =
                (!rd.buffer.hostVisible) ? 1 : FLY_FRAME_IN_FLIGHT_COUNT;
            for (u32 i = 0; i < count; i++)
            {
                RHI::DestroyBuffer(device_, buffers_[rd.arrayIndex + i]);
            }
        }
        else if (rd.type == ResourceType::Texture2D)
        {
            if (rd.texture2D.external || handle.version != 0 ||
                handle.id == FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
            {
                continue;
            }
            RHI::DestroyTexture2D(device_, textures_[rd.arrayIndex]);
        }
    }
}

void FrameGraph::ResizeDynamicTextures()
{
    for (HashTrie<ResourceHandle, ResourceDescriptor>::Node* node : resources_)
    {
        ResourceHandle handle = node->key;
        ResourceDescriptor& rd = node->value;

        if (rd.type == ResourceType::Texture2D && handle.version == 0 &&
            handle.id != FLY_SWAPCHAIN_TEXTURE_HANDLE_ID &&
            rd.texture2D.sizeType ==
                FrameGraph::TextureSizeType::SwapchainRelative)
        {
            u32 width = static_cast<u32>(device_.swapchainWidth *
                                         rd.texture2D.relativeSize.x);
            u32 height = static_cast<u32>(device_.swapchainHeight *
                                          rd.texture2D.relativeSize.y);

            RHI::DestroyTexture2D(device_, textures_[rd.arrayIndex]);

            bool res = RHI::CreateTexture2D(
                device_, rd.texture2D.usage, rd.data, width, height,
                rd.texture2D.format, rd.texture2D.filterMode,
                rd.texture2D.wrapMode, textures_[rd.arrayIndex]);
            FLY_ASSERT(res);
        }
    }
}

void FrameGraph::Execute()
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::BeginRenderFrame(device_);
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device_);

    // TODO: Memory barriers, image layout transitions, compute pass

    for (PassNode* pass : passes_)
    {
        if (pass->type == PassNode::Type::Transfer ||
            pass->type == PassNode::Type::Compute)
        {
            pass->recordCallbackImpl(cmd, resources_, *pass);
        }
        else if (pass->type == PassNode::Type::Graphics)
        {

            AttachmentCount count = CountAttachments(pass, resources_);
            FLY_ASSERT(count.color);
            FLY_ASSERT(count.depth <= 1);

            VkRenderingAttachmentInfo* attachments = FLY_PUSH_ARENA(
                arena, VkRenderingAttachmentInfo, count.color + count.depth);

            u32 i = 0;
            VkRect2D renderArea;
            renderArea.offset = {0, 0};
            for (ResourceHandle rh : pass->outputs)
            {
                const FrameGraph::ResourceDescriptor* rd = resources_.Find(rh);
                FLY_ASSERT(rd);

                if (rd->type == FrameGraph::ResourceType::Texture2D)
                {
                    VkImageView imageView;
                    if (rh.id == FLY_SWAPCHAIN_TEXTURE_HANDLE_ID)
                    {
                        const SwapchainTexture& swapchainTexture =
                            RenderFrameSwapchainTexture(device_);
                        imageView = swapchainTexture.imageView;
                        renderArea.extent = {device_.swapchainWidth,
                                             device_.swapchainHeight};
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

            vkCmdBeginRendering(cmd.handle, &renderInfo);
            pass->recordCallbackImpl(cmd, resources_, *pass);
            vkCmdEndRendering(cmd.handle);
        }
    }

    RHI::EndRenderFrame(device_);
    ArenaPopToMarker(arena, marker);
}

u32 FrameGraph::GetSwapchainIndex() const { return device_.frameIndex; }

} // namespace RHI
} // namespace Fly
