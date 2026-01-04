#ifndef FLY_DDS_IMAGE_H
#define FLY_DDS_IMAGE_H

#include "core/string8.h"
#include "core/types.h"

namespace Fly
{

struct Image;

namespace DDS
{

struct PixelFormat
{
    u32 size;
    u32 flags;
    u32 fourCC;
    u32 rgbBitCount;
    u32 rBitMask;
    u32 gBitMask;
    u32 bBitMask;
    u32 aBitMask;
};

struct HeaderDXT10
{
    u32 dxgiFormat;
    u32 resourceDimension;
    u32 miscFlag;
    u32 arraySize;
    u32 miscFlag2;
};

struct Header
{
    u32 size;
    u32 flags;
    u32 height;
    u32 width;
    u32 pitchOrLinearSize;
    u32 depth;
    u32 mipMapCount;
    u32 reserved1[11];
    PixelFormat pixelFormat;
    u32 caps;
    u32 caps2;
    u32 caps3;
    u32 caps4;
    u32 reserved2;
};

bool LoadDDSImage(String8 path, Image& image);

} // namespace DDS
} // namespace Fly

#endif /* FLY_DDS_IMAGE_H */
