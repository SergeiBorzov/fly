#ifndef HLS_ASSETS_IMPORT_GLTF_H
#define HLS_ASSETS_IMPORT_GLTF_H

#include <cgltf.h>

#include "rhi/buffer.h"

namespace Hls
{

struct Geometry
{
    Hls::Buffer vertexBuffer;
    Hls::Buffer indexBuffer;
    u64 indexOffset = 0;
    u32 indexCount = 0;
};

struct SceneData
{
    Geometry* geometries = nullptr;
    u32 geometryCount = 0;
};

bool LoadGltf(const char* path, const cgltf_options& options,
              cgltf_data** data);
bool CopyGltfToDevice(Device& device, cgltf_data* data, SceneData& sceneData);
void FreeGltf(cgltf_data* data);

bool LoadGltfToDevice(Device& device, const char* path,
                      const cgltf_options& options, SceneData& sceneData);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_GLTF_H */
