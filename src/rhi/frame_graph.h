#ifndef FLY_RHI_FRAME_GRAPH_H
#define FLY_RHI_FRAME_GRAPH_H

#include "core/arena.h"
#include "core/hash_trie.h"
#include "core/list.h"

#include "buffer.h"
#include "texture.h"

namespace Fly
{

namespace RHI
{

struct FrameGraph
{
    using ResourceHandle = u32;

    enum class ResourceAccess
    {
        Read,
        Write,
        ReadWrite
    };

    enum class ResourceType
    {
        Attachment = 0,
        Reference = 1,
        Texture2D = 2,
        Buffer = 3
    };

    struct PassNode;
    struct ResourceDescriptor;

    struct BufferCreateInfo
    {
        VkBufferUsageFlags usage;
        bool hostVisible;
    };

    struct Texture2DCreateInfo
    {
        VkImageUsageFlags usage;
        uint32_t width;
        uint32_t height;
        VkFormat format;
        RHI::Sampler::WrapMode wrapMode;
        RHI::Sampler::FilterMode filterMode;
    };

    struct AttachmentCreateInfo
    {
        VkImageView imageView;
        VkImageLayout imageLayout;
        VkAttachmentLoadOp loadOp;
        VkAttachmentStoreOp storeOp;
        VkClearValue clearValue;
    };

    struct ReferenceCreateInfo
    {
        ResourceDescriptor* rd;
    };

    struct ResourceDescriptor
    {
        union
        {
            Texture2DCreateInfo texture2D;
            BufferCreateInfo buffer;
            ReferenceCreateInfo reference;
        };

        void* data;
        u64 dataSize;
        ResourceType type;
        ResourceAccess access;
        bool isExternal;
    };

    struct Builder
    {
        PassNode* currentPass;
        HashTrie<u32, ResourceDescriptor> resourceDescriptors;
        u32 resourceCount = 0;
    };

    template <typename T>
    using BuildFunction = void (*)(Arena&, Builder&, T&, void*);

    template <typename T>
    using PassFunction = void (*)(T&);

    struct PassNode
    {
        using BuildFunctionImpl = void (*)(Arena&, FrameGraph::Builder&,
                                           PassNode&);
        using RecordFunctionImpl = void (*)(PassNode&);

        BuildFunctionImpl buildCallbackImpl;
        RecordFunctionImpl recordCallbackImpl;
        List<PassNode*> edges;
        List<ResourceHandle> inputs;
        List<ResourceHandle> outputs;
        void* pUserData;
        const char* name;
        bool isRootPass;
    };

    template <typename T>
    struct Pass : public PassNode
    {

        static void BuildImpl(Arena& arena, FrameGraph::Builder& builder,
                              PassNode& base)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass.buildCallback(arena, builder, pass.context, pass.pUserData);
        }

        static void RecordImpl(PassNode& base)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass.recordCallback(pass.context);
        }

        T context;
        BuildFunction<T> buildCallback;
        PassFunction<T> recordCallback;
    };

    template <typename T>
    void AddPass(Arena& arena, const char* name, BuildFunction<T> buildCallback,
                 PassFunction<T> recordCallback, void* pUserData,
                 bool isRootPass = false)
    {
        Pass<T>* pass = FLY_PUSH_ARENA(arena, Pass<T>, 1);
        pass->buildCallback = buildCallback;
        pass->buildCallbackImpl = Pass<T>::BuildImpl;
        pass->recordCallback = recordCallback;
        pass->recordCallbackImpl = Pass<T>::RecordImpl;
        pass->pUserData = pUserData;
        pass->name = name;
        pass->edges = {};
        pass->inputs = {};
        pass->outputs = {};
        pass->isRootPass = isRootPass;

        passes_.InsertBack(arena, pass);
    }

    bool Build(Arena& arena, RHI::Device& device);
    void Execute(RHI::Device& device);

private:
    List<PassNode*> passes_;
};

u32 SwapchainTextureDescriptor(Arena& arena, FrameGraph::Builder& builder);

u32 CreateBufferDescriptor(Arena& arena, FrameGraph::Builder& builder,
                           VkBufferUsageFlags usage, bool hostVisible,
                           void* data, u64 dataSize,
                           FrameGraph::ResourceAccess accessMask);
u32 CreateReference(Arena& arena, FrameGraph::Builder& builder,
                    FrameGraph::ResourceHandle resourceHandle,
                    FrameGraph::ResourceAccess accessMask);

u32 CreateTextureDescriptor(Arena& arena, FrameGraph::Builder& builder,
                            VkImageUsageFlags usage, void* data, u64 dataSize,
                            u32 width, u32 height, VkFormat format,
                            Sampler::FilterMode filterMode,
                            Sampler::WrapMode wrapMode,
                            FrameGraph::ResourceAccess accessMask);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_FRAME_GRAPH_H */
