#ifndef HLS_FILESYSTEM_H
#define HLS_FILESYSTEM_H

#include "string8.h"

struct Arena;

namespace Hls
{

String8 GetParentDirectoryPath(String8 path);
String8 AppendPaths(Arena& arena, String8* paths, u32 count);
String8 ReadFileToString(Arena& arena, String8 filename, u32 align = 1,
                         bool binaryMode = true);
} // namespace Hls

#endif /* HLS_FILESYSTEM_H */
