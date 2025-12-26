#ifndef FLY_ASSETS_SCENE_HEADER_H
#define FLY_ASSETS_SCENE_HEADER_H

#include "core/types.h"
#include "math/quat.h"
#include "math/vec.h"

namespace Fly
{

struct SerializedSceneNode
{
    Math::Quat localRotation = Math::Quat();
    Math::Vec3 localPosition = Math::Vec3(0.0f);
    Math::Vec3 localScale = Math::Vec3(1.0f);
    i64 meshIndex = -1;
    i64 parentIndex = -1;
};

struct SceneFileHeader
{
    struct
    {
        u32 major;
        u32 minor;
        u32 patch;
    } version;
    u64 totalVertexCount = 0;
    u64 totalIndexCount = 0;
    u32 totalSubmeshCount = 0;
    u32 totalLodCount = 0;
    u32 textureCount = 0;
    u32 meshCount = 0;
    u32 nodeCount = 0;
};

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_HEADER_H */
