#ifndef HLS_DEMOS_COMMON_SCENE_H
#define HLS_DEMOS_COMMON_SCENE_H

#include "math/vec.h"

#include "rhi/index_buffer.h"
#include "rhi/storage_buffer.h"
#include "rhi/texture.h"

struct Arena;

namespace Hls
{

struct Submesh
{
    RHI::StorageBuffer vertexBuffer;
    u32 indexOffset = 0;
    u32 indexCount = 0;
    u32 materialIndex = 0;
};

struct Mesh
{
    Submesh* submeshes = nullptr;
    u32 submeshCount = 0;
};

struct TextureProperty
{
    Math::Vec2 offset = {0.0f, 0.0f};
    Math::Vec2 scale = {1.0f, 1.0f};
    u32 textureHandle = HLS_MAX_U32;
    f32 pad;
};

struct MaterialData
{
    TextureProperty albedoTexture;
};

struct Material
{
    u32 materialHandle = HLS_MAX_U32;
};

struct Scene
{
    RHI::IndexBuffer indexBuffer;
    RHI::StorageBuffer materialBuffer;
    Mesh* meshes = nullptr;
    Material* materials = nullptr;
    RHI::Texture* textures = nullptr;
    u32 materialCount = 0;
    u32 meshCount = 0;
    u32 textureCount = 0;
};

bool LoadTextureFromFile(RHI::Device& device, const char* path, VkFormat format,
                         RHI::Sampler::FilterMode filterMode,
                         RHI::Sampler::WrapMode wrapMode, u32 anisotropy,
                         RHI::Texture& texture);
bool LoadShaderFromSpv(RHI::Device& device, const char* path,
                       RHI::Shader& shader);
bool LoadSceneFromGLTF(Arena& arena, RHI::Device& device, const char* path,
                       Scene& scene);
void UnloadScene(RHI::Device& device, Scene& scene);

} // namespace Hls
#endif /* HLS_DEMOS_COMMON_SCENE_H */
