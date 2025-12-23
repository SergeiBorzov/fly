#ifndef FLY_FILESYSTEM_H
#define FLY_FILESYSTEM_H

#include "string8.h"

namespace Fly
{
struct Arena;

bool CreateDirectories(String8 path);
u8* ReadFileToByteArray(String8 path, u64& size, u32 align = 1);
String8 ReadFileToString(Arena& arena, String8 filename, u32 align = 1);
bool WriteStringToFile(String8 str, String8 path, bool append = false);

String8 CurrentWorkingDirectory(Arena& arena);
String8 ParentDirectory(String8 path);

} // namespace Fly

#endif /* FLY_FILESYSTEM_H */
