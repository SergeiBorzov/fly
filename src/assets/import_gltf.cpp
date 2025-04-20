#include "core/assert.h"
#include "core/memory.h"

#include "import_gltf.h"

#define CGLTF_MALLOC(size) (Hls::Alloc(size))
#define CGLTF_FREE(size) (Hls::Free(size))
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace Hls
{

bool ImportGLTF(const char* path, const cgltf_options& options,
                cgltf_data** data)
{
    HLS_ASSERT(path);
    if (cgltf_parse_file(&options, path, data) != cgltf_result_success)
    {
        return false;
    }

    if (cgltf_load_buffers(&options, *data, path) != cgltf_result_success)
    {
        return false;
    }

    return true;
}

void FreeGLTF(cgltf_data* data) { cgltf_free(data); }

} // namespace Hls
