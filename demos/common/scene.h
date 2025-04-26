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
    StorageBuffer vertexBuffer;
    IndexBuffer indexBuffer;
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
    Hls::Texture texture;
    Math::Vec2 offset = {0.0f, 0.0f};
    Math::Vec2 scale = {1.0f, 1.0f};
};

struct Material
{
    TextureProperty albedoTexture = {};
};

struct Scene
{
    Mesh* meshes = nullptr;
    Material* materials = nullptr;
    Texture* textures = nullptr;
    u32 materialCount = 0;
    u32 meshCount = 0;
    u32 textureCount = 0;
};

bool LoadSceneFromGLTF(Arena& arena, Device& device, const char* path,
                       Scene& scene);
void UnloadScene(Device& device, Scene& scene);

} // namespace Hls
#endif /* HLS_DEMOS_COMMON_SCENE_H */
