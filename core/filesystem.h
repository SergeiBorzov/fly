#ifndef HLS_FILESYSTEM_H
#define HLS_FILESYSTEM_H

#include "types.h"

struct Arena;

const char* GetBinaryDirectoryPath(Arena& arena);
HlsResult SetEnv(const char* name, const char* value);


#endif /* HLS_FILESYSTEM_H */
