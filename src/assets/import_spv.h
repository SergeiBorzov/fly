#ifndef FLY_ASSETS_IMPORT_SPV_H
#define FLY_ASSETS_IMPORT_SPV_H

#include "core/string8.h"


namespace Fly
{
struct Arena;
struct String8;

String8 LoadSpvFromFile(Arena& arena, String8 path);

} // namespace Fly

#endif /* FLY_ASSETS_IMPORT_SPV_H */
