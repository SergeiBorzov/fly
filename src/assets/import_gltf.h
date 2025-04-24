#ifndef HLS_ASSETS_IMPORT_GLTF_H
#define HLS_ASSETS_IMPORT_GLTF_H

#include <cgltf.h>

#include "rhi/storage_buffer.h"
#include "rhi/index_buffer.h"

struct Arena;

namespace Hls
{

struct Geometry
{
    Hls::StorageBuffer vertexBuffer;
    Hls::IndexBuffer indexBuffer;
};

struct Mesh
{
    Geometry* geometries = nullptr;
    u32 geometryCount = 0;
};

struct SceneData
{
    Mesh* meshes = nullptr;
    u32 meshCount = 0;
};

bool LoadGltf(const char* path, const cgltf_options& options,
              cgltf_data** data);
bool CopyGltfToDevice(Arena& arena, Device& device, cgltf_data* data,
                      SceneData& sceneData);
void FreeGltf(cgltf_data* data);

bool LoadGltfToDevice(Arena& arena, Device& device, const char* path,
                      const cgltf_options& options, SceneData& sceneData);

void FreeDeviceSceneData(Device& device, SceneData& sceneData);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_GLTF_H */
