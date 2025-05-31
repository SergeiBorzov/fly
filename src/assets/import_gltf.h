#ifndef FLY_ASSETS_IMPORT_GLTF_H
#define FLY_ASSETS_IMPORT_GLTF_H

#include <cgltf.h>

namespace Fly
{

bool LoadGltfFromFile(const char* path, const cgltf_options& options,
              cgltf_data** data);
void FreeGltf(cgltf_data* data);

} // namespace Fly

#endif /* FLY_ASSETS_IMPORT_GLTF_H */
