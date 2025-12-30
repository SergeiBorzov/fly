#ifndef FLY_ASSETS_SCENE_SCENE_H
#define FLY_ASSETS_SCENE_SCENE_H

#include "core/types.h"

#include "math/transform.h"

#include "assets/geometry/mesh.h"

#include "rhi/buffer.h"
#include "rhi/texture.h"

namespace Fly
{

struct SceneNode
{
    Math::Transform transform{};
    Mesh* mesh = nullptr;
};

struct PBRMaterial
{
    Math::Vec4 baseColor = Math::Vec4(1.0f);
    u32 baseColorTextureBindlessHandle = FLY_MAX_U32;
    u32 normalTextureBindlessHandle = FLY_MAX_U32;
    u32 ormTextureBindlessHandle = FLY_MAX_U32;
    u32 pad = 0;
};

struct Scene
{
    RHI::Buffer vertexBuffer{};
    RHI::Buffer indexBuffer{};
    RHI::Buffer pbrMaterialBuffer{};
    RHI::Texture whiteTexture{};
    RHI::Texture flatNormalTexture{};
    RHI::Texture* textures = nullptr;
    Mesh* meshes = nullptr;
    SceneNode* nodes = nullptr;
    u32 nodeCount = 0;
    u32 textureCount = 0;
    u32 meshCount = 0;
    u32 materialCount = 0;
};

bool ImportScene(String8 path, RHI::Device& device, Scene& scene);
void DestroyScene(RHI::Device& device, Scene& scene);

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_SCENE_H */
