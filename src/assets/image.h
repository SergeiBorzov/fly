#ifndef FLY_IMAGE_H
#define FLY_IMAGE_H

#include "core/types.h"

namespace Fly
{

enum class ImageStorageType : u8
{
    Byte,
    Half,
    Block8,
    Block16
};

struct ImageHeader
{
    ImageStorageType storageType;
    u8 channelCount;
    u8 mipCount;
    u8 layerCount;
};

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
    u64 size = 0;
};

struct Image
{
    u8* mem = nullptr;
    u8* data = nullptr;
    u32 width = 0;
    u32 height = 0;
    u8 mipCount = 0;
    u8 channelCount = 0;
    u8 layerCount = 0;
    ImageStorageType storageType;
};

u32 GetImageStorageTypeSize(ImageStorageType type);
bool GetImageMipLevel(Image& image, u32 layer, u32 mipLevel, Mip& mip);
void FreeImage(Image& image);

} // namespace Fly

#endif /* FLY_IMAGE_H */
