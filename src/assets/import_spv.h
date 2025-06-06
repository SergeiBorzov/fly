#ifndef FLY_ASSETS_IMPORT_SPV_H
#define FLY_ASSETS_IMPORT_SPV_H

#include "core/string8.h"


namespace Fly
{
struct Arena;
struct Path;

String8 LoadSpvFromFile(Arena& arena, const char* path);
String8 LoadSpvFromFile(Arena& arena, const Path& path);

} // namespace Fly

#endif /* FLY_ASSETS_IMPORT_SPV_H */
