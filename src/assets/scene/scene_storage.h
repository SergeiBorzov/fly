#ifndef FLY_ASSETS_SCENE_STORAGE_H
#define FLY_ASSETS_SCENE_STORAGE_H

#include "math/transform.h"

namespace Fly
{

struct Geometry;
struct Image;

struct SceneStorage
{
    Math::Transform root{};
    Math::Transform* nodes = nullptr;
    Geometry* geometries = nullptr;
    Image* images = nullptr;
    void* vertexBuffer = nullptr;
    u32* indexBuffer = nullptr;
    u64 vertexBufferSize = 0;
    u64 indexBufferSize = 0;
    u32 nodeCount = 0;
    u32 geometryCount = 0;
    u32 imageCount = 0;
};

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_STORAGE_H */
