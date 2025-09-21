#ifndef FLY_IMAGE_BC_H
#define FLY_IMAGE_BC_H

#include "core/types.h"

namespace Fly
{

enum class CodecType
{
    Invalid,
    BC1,
    BC3,
    BC4,
    BC5,
};

struct Image;

// BC1, BC4
u64 SizeBlock8(u32 width, u32 height);
// BC3, BC5, BC6, BC7
u64 SizeBlock16(u32 width, u32 height);

bool CompressImage(u8* dst, u64 dstSize, const Image& image, CodecType codec);

} // namespace Fly

#endif /* FLY_TEXTURE_COMPRESSION_H */
