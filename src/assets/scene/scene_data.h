#ifndef FLY_ASSETS_SCENE_DATA_H
#define FLY_ASSETS_SCENE_DATA_H

#include "math/transform.h"

namespace Fly
{

struct Geometry;
struct Image;

struct SceneNode
{
    Math::Quat localRotation = Math::Quat();
    Math::Vec3 localPosition = Math::Vec3(0.0f);
    Math::Vec3 localScale = Math::Vec3(1.0f);
    i64 geometryIndex = -1;
    i64 parentIndex = -1;
};

struct SceneData
{
    SceneNode* nodes = nullptr;
    Geometry* geometries = nullptr;
    Image* images = nullptr;
    u32 nodeCount = 0;
    u32 geometryCount = 0;
    u32 imageCount = 0;
};

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_DATA_H */
