#ifndef FLY_DEMOS_COMMON_SCENE_H
#define FLY_DEMOS_COMMON_SCENE_H

#include "math/mat.h"

#include "rhi/buffer.h"
#include "rhi/texture.h"

namespace Fly
{
struct Arena;

// CPU based draw call structures
struct Submesh
{
    u32 vertexBufferIndex = 0;
    u32 indexOffset = 0;
    u32 indexCount = 0;
    u32 materialIndex = 0;
};

struct Mesh
{
    Submesh* submeshes = nullptr;
    u32 submeshCount = 0;
};

struct DirectDrawData
{
    Submesh* submeshes = nullptr;
    Mesh* meshes = nullptr;
    u32 meshCount = 0;
    u32 submeshCount = 0;
};
//

// Indirect draw structures
struct BoundingSphereDraw
{
    Math::Vec3 center;
    f32 radius = 0.0f;
    u32 indexCount = 0;
    u32 indexOffset = 0;
};

struct MeshNode
{
    Math::Mat4 model;
    Mesh* mesh = nullptr;
};

struct InstanceData
{
    Math::Mat4 model;
    u32 meshDataIndex = 0;
    u32 padding[3] = {0};
};

struct MeshData
{
    // bindless handle to locate vb in shader
    u32 materialIndex = 0;
    u32 vertexBufferIndex = 0;
    u32 boundingSphereDrawIndex = 0;
};

struct IndirectDrawData
{
    RHI::Buffer instanceDataBuffer;
    RHI::Buffer boundingSphereDrawBuffer;
    RHI::Buffer meshDataBuffer;
};
//

struct TextureProperty
{
    Math::Vec2 offset = {0.0f, 0.0f};
    Math::Vec2 scale = {1.0f, 1.0f};
    u32 textureHandle = FLY_MAX_U32;
    u32 pad;
};

struct PBRMaterialData
{
    TextureProperty albedoTexture;
};

struct Scene
{
    IndirectDrawData indirectDrawData;
    DirectDrawData directDrawData;
    RHI::Buffer indexBuffer;
    RHI::Buffer materialBuffer;
    RHI::Buffer* vertexBuffers = nullptr;
    RHI::Texture* textures = nullptr;
    MeshNode* meshNodes = nullptr;
    u32 meshNodeCount = 0;
    u32 vertexBufferCount = 0;
    u32 materialCount = 0;
    u32 textureCount = 0;
};

bool LoadSceneFromGLTF(Arena& arena, RHI::Device& device, const char* path,
                       Scene& scene);
void UnloadScene(RHI::Device& device, Scene& scene);

} // namespace Fly
#endif /* FLY_DEMOS_COMMON_SCENE_H */
