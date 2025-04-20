#ifndef HLS_ASSETS_IMPORT_GLTF_H
#define HLS_ASSETS_IMPORT_GLTF_H

#include <cgltf.h>

namespace Hls
{

bool ImportGLTF(const char* path, const cgltf_options& options,
                cgltf_data** data);
void FreeGLTF(cgltf_data* data);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_GLTF_H */
