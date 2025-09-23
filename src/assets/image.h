#ifndef FLY_IMAGE_H
#define FLY_IMAGE_H

#include "core/types.h"

namespace Fly
{

struct MipRow
{
    u64 offset;
    u32 width;
    u32 height;
};

struct Mip
{
    void* data = nullptr;
    u32 width = 0;
    u32 height = 0;
};

struct Image
{
    u8* mem = nullptr;
    u8* data = nullptr;
    u32 mipCount = 0;
    u32 width = 0;
    u32 height = 0;
    u32 channelCount = 0;
};

bool GetMipLevel(Image& image, u32 mipLevel, Mip& mip);
void FreeImage(Image& image);

} // namespace Fly

#endif /* FLY_IMAGE_H */
