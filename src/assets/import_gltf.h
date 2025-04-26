#ifndef HLS_ASSETS_IMPORT_GLTF_H
#define HLS_ASSETS_IMPORT_GLTF_H

#include "math/vec.h"

#include "rhi/index_buffer.h"
#include "rhi/storage_buffer.h"
#include "rhi/texture.h"

#include <cgltf.h>

namespace Hls
{

bool LoadGltf(const char* path, const cgltf_options& options,
              cgltf_data** data);
void FreeGltf(cgltf_data* data);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_GLTF_H */
