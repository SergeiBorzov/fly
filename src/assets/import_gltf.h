#ifndef HLS_ASSETS_IMPORT_GLTF_H
#define HLS_ASSETS_IMPORT_GLTF_H

#include <cgltf.h>

namespace Hls
{

bool LoadGltfFromFile(const char* path, const cgltf_options& options,
              cgltf_data** data);
void FreeGltf(cgltf_data* data);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_GLTF_H */
