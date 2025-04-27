#ifndef HLS_ASSETS_IMPORT_SPV_H
#define HLS_ASSETS_IMPORT_SPV_H

#include "core/string8.h"

struct Arena;

namespace Hls
{

struct Path;

String8 LoadSpvFromFile(Arena& arena, const char* path);
String8 LoadSpvFromFile(Arena& arena, const Path& path);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_SPV_H */
