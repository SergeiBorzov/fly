#ifndef FLY_RHI_FRAME_GRAPH_H
#define FLY_RHI_FRAME_GRAPH_H

#include "core/arena.h"
#include "core/hash.h"
#include "core/hash_trie.h"
#include "core/list.h"

#include "buffer.h"
#include "texture.h"

#define FLY_SWAPCHAIN_TEXTURE_HANDLE_ID 0

namespace Fly
{

namespace RHI
{

struct ResourceHandle
{
    u32 id;
    u32 version;

    inline bool operator==(const ResourceHandle& other) const
    {
        return (id == other.id) && (version == other.version);
    }

    inline bool operator!=(const ResourceHandle& other) const
    {
        return (id != other.id) || (version != other.version);
    }
};

} // namespace RHI

template <>
struct Hash<RHI::ResourceHandle>
{
    inline u64 operator()(RHI::ResourceHandle handle)
    {
        return (static_cast<u64>(handle.id) << 32) | handle.version;
    }
};

namespace RHI
{

struct FrameGraph
{
    struct BufferHandle
    {
        ResourceHandle handle;

        inline bool operator==(const BufferHandle& other) const
        {
            return handle == other.handle;
        }

        inline bool operator!=(const BufferHandle& other) const
        {
            return handle != other.handle;
        }
    };

    struct TextureHandle
    {
        static const TextureHandle sBackBuffer;
        ResourceHandle handle;

        inline bool operator==(const TextureHandle& other) const
        {
            return handle == other.handle;
        }

        inline bool operator!=(const TextureHandle& other) const
        {
            return handle != other.handle;
        }
    };

    enum class ResourceType
    {
        Buffer,
        Texture2D
    };

    enum class ResourceAccess
    {
        Unknown,
        Read,
        Write,
        ReadWrite
    };

    struct PassNode;
    struct ResourceDescriptor;

    struct BufferCreateInfo
    {
        VkBufferUsageFlags usage;
        bool hostVisible;
    };

    enum class TextureSizeType
    {
        Fixed,
        ViewportRelative,
    };

    struct Texture2DCreateInfo
    {
        VkImageUsageFlags usage;

        union
        {
            struct
            {
                u32 width;
                u32 height;
            };

            struct
            {
                f32 x;
                f32 y;
            } relativeSize;
        };

        u32 index;

        TextureSizeType sizeType;
        VkFormat format;
        RHI::Sampler::WrapMode wrapMode;
        RHI::Sampler::FilterMode filterMode;
        VkAttachmentLoadOp loadOp;
        VkAttachmentStoreOp storeOp;
        VkClearValue clearValue;
    };

    struct ResourceDescriptor
    {
        union
        {
            Texture2DCreateInfo texture2D;
            BufferCreateInfo buffer;
        };

        void* data;
        u64 dataSize;
        i32 arrayIndex;
        ResourceType type;
        ResourceAccess access;
        bool isExternal;
    };

    struct Builder
    {
        Builder(FrameGraph& inFrameGraph) : frameGraph(inFrameGraph) {}

        FrameGraph& frameGraph;
        PassNode* currentPass;
    };

    template <typename T>
    using BuildFunction = void (*)(Arena&, Builder&, T&, void*);

    template <typename T>
    using RecordFunction = void (*)(RHI::CommandBuffer& cmd, const T&, void*);

    struct PassNode
    {
        enum class Type
        {
            Graphics,
            Compute,
        };

        using BuildFunctionImpl = void (*)(Arena&, FrameGraph::Builder&,
                                           PassNode&);
        using RecordFunctionImpl = void (*)(RHI::CommandBuffer&, PassNode&);

        BuildFunctionImpl buildCallbackImpl;
        RecordFunctionImpl recordCallbackImpl;
        List<PassNode*> edges;
        List<ResourceHandle> inputs;
        List<ResourceHandle> outputs;
        void* userData;
        FrameGraph* frameGraph;
        const char* name;
        Type type;
        bool isRootPass;
    };

    template <typename T>
    struct Pass : public PassNode
    {
        static void BuildImpl(Arena& arena, FrameGraph::Builder& builder,
                              PassNode& base)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass.buildCallback(arena, builder, pass.context, pass.userData);
        }

        static void RecordImpl(RHI::CommandBuffer& cmd, PassNode& base)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass.recordCallback(cmd, pass.context, pass.userData);
        }

        T context;
        BuildFunction<T> buildCallback;
        RecordFunction<T> recordCallback;
    };

    template <typename T>
    void AddPass(Arena& arena, const char* name, PassNode::Type type,
                 BuildFunction<T> buildCallback,
                 RecordFunction<T> recordCallback, void* userData,
                 bool isRootPass = false)
    {
        Pass<T>* pass = FLY_PUSH_ARENA(arena, Pass<T>, 1);
        pass->buildCallback = buildCallback;
        pass->buildCallbackImpl = Pass<T>::BuildImpl;
        pass->recordCallback = recordCallback;
        pass->recordCallbackImpl = Pass<T>::RecordImpl;
        pass->type = type;
        pass->frameGraph = this;
        pass->userData = userData;
        pass->name = name;
        pass->edges = {};
        pass->inputs = {};
        pass->outputs = {};
        pass->isRootPass = isRootPass;

        passes_.InsertBack(arena, pass);
    }

    inline FrameGraph(RHI::Device& device) : device_(device) {}

    inline VkFormat GetSwapchainFormat() const
    {
        return device_.surfaceFormat.format;
    }

    void GetSwapchainSize(u32& width, u32& height);

    bool Build(Arena& arena);
    void Execute();
    void Destroy();

    HashTrie<ResourceHandle, ResourceDescriptor> resources;

private:
    RHI::Device& device_;
    List<PassNode*> passes_;
    RHI::Buffer* buffers_;
    RHI::Texture2D* textures_;
    u32 bufferCount_ = 0;
    u32 textureCount_ = 0;
};

FrameGraph::BufferHandle CreateBuffer(Arena& arena,
                                      FrameGraph::Builder& builder,
                                      VkBufferUsageFlags usage,
                                      bool hostVisible, void* data,
                                      u64 dataSize);

FrameGraph::TextureHandle
CreateTexture2D(Arena& arena, FrameGraph::Builder& builder,
                VkImageUsageFlags usage, void* data, u64 dataSize, u32 width,
                u32 height, VkFormat format, Sampler::FilterMode filterMode,
                Sampler::WrapMode wrapMode);

FrameGraph::TextureHandle
ColorAttachment(Arena& arena, FrameGraph::Builder& builder, u32 index,
                FrameGraph::TextureHandle textureHandle,
                VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f});

FrameGraph::TextureHandle
DepthAttachment(Arena& arena, FrameGraph::Builder& builder,
                FrameGraph::TextureHandle textureHandle,
                VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                VkClearDepthStencilValue clearDepthStencil = {1.0f, 0});
} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_FRAME_GRAPH_H */
