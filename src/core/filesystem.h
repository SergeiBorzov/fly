#ifndef HLS_FILESYSTEM_H
#define HLS_FILESYSTEM_H

#include "string8.h"

struct Arena;

namespace Hls
{
String8 ReadFileToString(Arena& arena, const char* filename, u32 align = 1,
                         bool binaryMode = true);
}

#endif /* HLS_FILESYSTEM_H */
