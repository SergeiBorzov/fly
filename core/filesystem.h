#ifndef HLS_FILESYSTEM_H
#define HLS_FILESYSTEM_H

#include "types.h"

struct Arena;

const char* GetBinaryDirectoryPath(Arena& arena);
bool SetEnv(const char* name, const char* value);
char* ReadFileToString(Arena& arena, const char* filename, u64* size, u32 align = 1);


#endif /* HLS_FILESYSTEM_H */
