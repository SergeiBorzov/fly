#ifndef FLY_IMAGE_BC_H
#define FLY_IMAGE_BC_H

#include "core/types.h"

namespace Fly
{
    struct Image;
    
    void CompressImageBC1(u8* dest, u64 destSize, const Image& image);
    void CompressImageBC3(u8* dest, u64 destSize, const Image& image);
    void CompressImageBC4(u8* dest, u64 destSize, const Image& image);
    void CompressImageBC5(u8* dest, u64 destSize, const Image& image);
}

#endif /* FLY_TEXTURE_COMPRESSION_H */
