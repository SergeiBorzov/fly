#ifndef FLY_IMAGE_BC_H
#define FLY_IMAGE_BC_H

#include "core/types.h"

namespace Fly
{
struct Image;

enum class CodecType
{
    Invalid,
    BC1,
    BC3,
    BC4,
    BC5,
};

const char* CodecToExtension(CodecType codec);
u64 GetCompressedImageSize(u32 width, u32 height);
bool CompressImage(u8* dst, u64 dstSize, const Image& image, CodecType codec);
u8* CompressImage(const Image& image, CodecType codec, bool generateMips,
                  u64& size);

} // namespace Fly

#endif /* FLY_TEXTURE_COMPRESSION_H */
