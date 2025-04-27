#include "core/assert.h"
#include "core/log.h"
#include "core/memory.h"

#include "import_gltf.h"
#include "import_image.h"

#define CGLTF_MALLOC(size) (Hls::Alloc(size))
#define CGLTF_FREE(size) (Hls::Free(size))
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace Hls
{

bool LoadGltfFromFile(const char* path, const cgltf_options& options,
                      cgltf_data** data)
{
    HLS_ASSERT(path);
    if (cgltf_parse_file(&options, path, data) != cgltf_result_success)
    {
        HLS_ERROR("Failed to parse gltf file: %s", path);
        return false;
    }

    if (cgltf_load_buffers(&options, *data, path) != cgltf_result_success)
    {
        HLS_ERROR("Failed to load gltf buffers, file: %s", path);
        return false;
    }

    return true;
}

void FreeGltf(cgltf_data* data)
{
    HLS_ASSERT(data);
    cgltf_free(data);
}

} // namespace Hls
