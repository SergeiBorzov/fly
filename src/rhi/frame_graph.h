#ifndef FLY_RHI_FRAME_GRAPH_H
#define FLY_RHI_FRAME_GRAPH_H

#include "core/arena.h"

#include "buffer.h"
#include "texture.h"

namespace Fly
{

namespace RHI
{

struct FrameGraph
{
    enum class ResourceAccess
    {
        Read = 0,
        Write = 1,
        ReadWrite = 2
    };

    struct Builder
    {
        struct BufferCreateInfo
        {
            VkBufferUsageFlags usage;
            bool hostVisible;
            void* data;
            u64 dataSize;
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

        struct ResourceDescriptor
        {
            enum class Type
            {
                Buffer,
                Texture2D,
            };

            union
            {
                Texture2DCreateInfo texture2D;
                BufferCreateInfo buffer;
            };

            ResourceDescriptor* next;
            void* data;
            u64 dataSize;
            Type type;
            ResourceAccess accessMask;
        };

        ResourceDescriptor* resourceDescriptors;
    };

    template <typename T>
    using BuildFunction = void (*)(Arena&, Builder&, T&, void*);

    template <typename T>
    using PassFunction = void (*)(T&);

    struct ResourceInfo
    {
        u32 id;
        ResourceAccess access;
    };

    struct PassNode
    {
        using BuildFunctionImpl = void (*)(Arena&, FrameGraph::Builder&,
                                           PassNode*, void*);
        using RecordFunctionImpl = void (*)(PassNode*);

        BuildFunctionImpl buildCallback;
        RecordFunctionImpl recordCallback;
        PassNode* parents;
        PassNode* children;
        PassNode* next;
        ResourceInfo* resources;
        void* pUserData;
        const char* name;
    };

    template <typename T>
    struct Pass : public PassNode
    {

        static void BuildImpl(Arena& arena, FrameGraph::Builder& builder,
                              PassNode& base, void* pUserData)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass->buildCallback(arena, builder, pass->context, pUserData);
        }

        static void RecordImpl(PassNode* base)
        {
            Pass<T>& pass = static_cast<Pass<T>&>(base);
            pass->recordCallback(pass->context);
        }

        T context;
        BuildFunction<T> buildCallback;
        PassFunction<T> recordCallback;
    };

    template <typename T>
    void AddPass(Arena& arena, const char* name, BuildFunction<T> buildCallback,
                 PassFunction<T> recordCallback, void* pUserData)
    {
        Pass<T>* pass = FLY_PUSH_ARENA(arena, Pass<T>, 1);
        pass->buildCallback = buildCallback;
        pass->recordCallback = recordCallback;
        pass->pUserData = pUserData;
        pass->name = name;
        pass->parents = nullptr;
        pass->children = nullptr;
        pass->resources = nullptr;

        pass->next = head_;
        head_ = pass;
    }

    bool Build(Arena& arena);

private:
    PassNode* head_ = nullptr;
};

/* u32 CreateBufferDescriptor(FrameGraph::Builder& builder, */
/*                            VkBufferUsageFlags usage, bool hostVisible, */
/*                            void* data, u64 dataSize, */
/*                            FrameGraph::ResourceAccess accessMask) */
/* { */
/*     return 0; */
/* } */

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_FRAME_GRAPH_H */
