#ifndef FLY_ASSETS_SCENE_DATA_H
#define FLY_ASSETS_SCENE_DATA_H

#include "core/types.h"

namespace Fly
{

struct Geometry;
struct Image;
struct SerializedSceneNode;

struct SceneData
{
    SerializedSceneNode* nodes = nullptr;
    Geometry* geometries = nullptr;
    Image* images = nullptr;
    u32 nodeCount = 0;
    u32 geometryCount = 0;
    u32 imageCount = 0;
};

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_DATA_H */
