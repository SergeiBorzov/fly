#ifndef HLS_FILESYSTEM_H
#define HLS_FILESYSTEM_H

#include "types.h"

struct Arena;

char* ReadFileToString(Arena& arena, const char* filename, u64* size, u32 align = 1, bool binaryMode = true);

#endif /* HLS_FILESYSTEM_H */
