#ifndef FLY_IMAGE_H
#define FLY_IMAGE_H

#include "core/types.h"

namespace Fly
{

enum class ImageStorageType : u8
{
    BC1 = 0,
    BC3 = 1,
    BC4 = 2,
    BC5 = 3,
    Byte = 4,
    Half = 5,
    Float = 6,
};

struct ImageHeader
{
    u32 width = 0;
    u32 height = 0;
    u8 channelCount = 0;
    u8 layerCount = 0;
    u8 mipCount = 0;
    ImageStorageType storageType = ImageStorageType::Byte;
};

struct Image
{
    u8* data = nullptr;
    u32 width = 0;
    u32 height = 0;
    u8 channelCount = 0;
    u8 layerCount = 0;
    u8 mipCount = 0;
    ImageStorageType storageType = ImageStorageType::Byte;
};

u32 GetImageStorageTypeSize(ImageStorageType type);
u64 GetImageSize(const Image& image);
u64 GetImageSize(u32 width, u32 height, u8 channelCount, u8 layerCount,
                 u8 mipCount, ImageStorageType storageType);

} // namespace Fly

#endif /* FLY_IMAGE_H */
