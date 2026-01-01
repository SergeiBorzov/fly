#ifndef FLY_ASSETS_EXPORT_SCENE_H
#define FLY_ASSETS_EXPORT_SCENE_H

#include "core/string8.h"
#include "core/types.h"

#include "assets/geometry/geometry.h"
namespace Fly
{

struct SceneData;

struct SceneExportOptions
{
    f32 scale = 1.0f;
    bool flipForward = false;
    bool flipWindingOrder = false;
    CoordSystem coordSystem = CoordSystem::XYZ;
    bool exportNodes = true;
    bool exportMaterials = true;
};

bool CookScene(String8 path, const SceneExportOptions& options,
               SceneData& sceneStorage);
bool ExportScene(String8 path, const SceneData& sceneStorage);

} // namespace Fly

#endif /* FLY_ASSETS_COOK_SCENE_H */
