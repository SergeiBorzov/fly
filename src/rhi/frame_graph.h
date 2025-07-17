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
    friend struct Builder;

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

    struct PassNode;
    struct ResourceDescriptor;

    struct BufferCreateInfo
    {
        RHI::Buffer* external;
        u64 size;
        VkBufferUsageFlags usage;
        bool hostVisible;
    };

    enum class TextureSizeType
    {
        Fixed,
        SwapchainRelative,
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

        RHI::Texture2D* external;
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

        const void* data;
        i32 arrayIndex;
        ResourceType type;
    };

    struct ResourceMap
    {
        ResourceMap(const FrameGraph* frameGraph) : frameGraph_(frameGraph) {}

        RHI::Texture2D& GetTexture2D(TextureHandle textureHandle);
        RHI::Buffer& GetBuffer(BufferHandle bufferHandle);
        const RHI::Texture2D& GetTexture2D(TextureHandle textureHandle) const;
        const RHI::Buffer& GetBuffer(BufferHandle bufferHandle) const;

        inline ResourceDescriptor* Find(ResourceHandle resourceHandle)
        {
            return resources_.Find(resourceHandle);
        }

        inline const ResourceDescriptor*
        Find(ResourceHandle resourceHandle) const
        {
            return resources_.Find(resourceHandle);
        }

        inline ResourceDescriptor& Insert(Arena& arena, ResourceHandle rh,
                                          ResourceDescriptor rd)
        {
            return resources_.Insert(arena, rh, rd);
        }

        ResourceHandle GetNextHandle();

        inline HashTrie<ResourceHandle, ResourceDescriptor>::Iterator begin()
        {
            return resources_.begin();
        }

        inline HashTrie<ResourceHandle, ResourceDescriptor>::Iterator end()
        {
            return resources_.end();
        }

        inline HashTrie<ResourceHandle, ResourceDescriptor>::ConstIterator
        begin() const
        {
            return resources_.begin();
        }

        inline HashTrie<ResourceHandle, ResourceDescriptor>::ConstIterator
        end() const
        {
            return resources_.end();
        }

    private:
        HashTrie<ResourceHandle, ResourceDescriptor> resources_;
        const FrameGraph* frameGraph_;
        u32 nextHandle_ = 0;
    };

    struct Builder
    {
        friend struct FrameGraph;

        Builder(ResourceMap& resources) : resources_(resources) {}

        FrameGraph::BufferHandle CreateBuffer(Arena& arena,
                                              VkBufferUsageFlags usage,
                                              bool hostVisible, const void* data,
                                              u64 dataSize);

        FrameGraph::TextureHandle
        CreateTexture2D(Arena& arena, VkImageUsageFlags usage, const void* data,
                        u32 width, u32 height, VkFormat format,
                        Sampler::FilterMode filterMode,
                        Sampler::WrapMode wrapMode);

        FrameGraph::TextureHandle
        CreateTexture2D(Arena& arena, VkImageUsageFlags usage,
                        f32 relativeSizeX, f32 relativeSizeY, VkFormat format);

        FrameGraph::BufferHandle RegisterExternalBuffer(Arena& arena,
                                                        RHI::Buffer& buffer);

        FrameGraph::TextureHandle
        RegisterExternalTexture2D(Arena& arena, RHI::Texture2D& texture2D);

        FrameGraph::TextureHandle ColorAttachment(
            Arena& arena, u32 index, FrameGraph::TextureHandle textureHandle,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f});

        FrameGraph::TextureHandle DepthAttachment(
            Arena& arena, FrameGraph::TextureHandle textureHandle,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            VkClearDepthStencilValue clearDepthStencil = {1.0f, 0});

        FrameGraph::BufferHandle Read(Arena& arena,
                                      FrameGraph::BufferHandle handle);
        FrameGraph::BufferHandle Write(Arena& arena,
                                       FrameGraph::BufferHandle handle);
        FrameGraph::BufferHandle ReadWrite(Arena& arena,
                                           FrameGraph::BufferHandle handle);

        FrameGraph::TextureHandle Read(Arena& arena,
                                       FrameGraph::TextureHandle handle);
        FrameGraph::TextureHandle Write(Arena& arena,
                                        FrameGraph::TextureHandle handle);
        FrameGraph::TextureHandle ReadWrite(Arena& arena,
                                            FrameGraph::TextureHandle handle);

    private:
        ResourceMap& resources_;
        PassNode* currentPass_;
    };

    template <typename T>
    using BuildFunction = void (*)(Arena&, Builder&, T&, void*);

    template <typename T>
    using RecordFunction = void (*)(RHI::CommandBuffer& cmd, ResourceMap&,
                                    const T&, void*);

    struct PassNode
    {
        enum class Type
        {
            Graphics,
            Compute,
            Transfer
        };

        using BuildFunctionImpl = void (*)(Arena&, FrameGraph::Builder&,
                                           PassNode&);
        using RecordFunctionImpl = void (*)(RHI::CommandBuffer&, ResourceMap&,
                                            PassNode&);

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

        static void RecordImpl(RHI::CommandBuffer& cmd, ResourceMap& resources,
                               PassNode& base)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass.recordCallback(cmd, resources, pass.context, pass.userData);
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

    FrameGraph(RHI::Device& device);

    inline VkFormat GetSwapchainFormat() const
    {
        return device_.surfaceFormat.format;
    }

    u32 GetSwapchainIndex() const;

    bool Build(Arena& arena);
    void Execute();
    void Destroy();

    inline RHI::Texture2D& GetTexture2D(TextureHandle textureHandle)
    {
        return resources_.GetTexture2D(textureHandle);
    }

    inline RHI::Buffer& GetBuffer(BufferHandle bufferHandle)
    {
        return resources_.GetBuffer(bufferHandle);
    }

    inline const RHI::Texture2D& GetTexture2D(TextureHandle textureHandle) const
    {
        return resources_.GetTexture2D(textureHandle);
    }

    inline const RHI::Buffer& GetBuffer(BufferHandle bufferHandle) const
    {
        return resources_.GetBuffer(bufferHandle);
    }

    void ResizeDynamicTextures();

private:
    ResourceMap resources_;
    List<PassNode*> passes_;
    RHI::Buffer* buffers_;
    RHI::Texture2D* textures_;
    RHI::Device& device_;
    u32 bufferCount_ = 0;
    u32 textureCount_ = 0;
};

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_FRAME_GRAPH_H */
