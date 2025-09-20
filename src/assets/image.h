#ifndef FLY_IMAGE_H
#define FLY_IMAGE_H

#include "core/types.h"

namespace Fly
{

struct Image
{
    u8* data = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 channelCount = 0;
};

} // namespace Fly

#endif /* FLY_IMAGE_H */
