#ifndef FLY_ASSETS_SCENE_SCENE_H
#define FLY_ASSETS_SCENE_SCENE_H

#include "core/types.h"

#include "math/transform.h"

#include "assets/geometry/mesh.h"

#include "rhi/buffer.h"

namespace Fly
{

struct SceneNode
{
    Math::Transform transform{};
    Mesh* mesh = nullptr;
};

struct Scene
{
    RHI::Buffer vertexBuffer{};
    RHI::Buffer indexBuffer{};
    Mesh* meshes = nullptr;
    RHI::Texture* textures = nullptr;
    SceneNode* nodes = nullptr;
    u32 nodeCount = 0;
    u32 textureCount = 0;
    u32 meshCount = 0;
};

bool ImportScene(String8 path, RHI::Device& device, Scene& scene);
void DestroyScene(RHI::Device& device, Scene& scene);

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_SCENE_H */
