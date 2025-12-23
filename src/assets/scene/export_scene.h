#ifndef FLY_ASSETS_EXPORT_SCENE_H
#define FLY_ASSETS_EXPORT_SCENE_H

#include "core/string8.h"

namespace Fly
{

struct SceneStorage;

bool CookScene(String8 path, SceneStorage& sceneStorage);
bool ExportScene(String8 path, SceneStorage& sceneStorage);

} // namespace Fly

#endif /* FLY_ASSETS_COOK_SCENE_H */
