#include "core/assert.h"
#include "core/memory.h"

#include "image.h"
#include "transform_image.h"

#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_CATMULLROM
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_MITCHELL
#define STBIR_ASSERT(x) FLY_ASSERT(x)
#define STBIR_MALLOC(size, userData) (Fly::Alloc(size))
#define STBIR_FREE(ptr, userData) (Fly::Free(ptr))
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace Fly
{

bool ResizeImageSRGB(const Image& srcImage, Image& dstImage)
{
    return stbir_resize_uint8_srgb(
        srcImage.data, srcImage.width, srcImage.height, 0, dstImage.data,
        dstImage.width, dstImage.height, 0,
        static_cast<stbir_pixel_layout>(dstImage.channelCount));
}

bool ResizeImageLinear(const Image& srcImage, Image& dstImage)
{
    return stbir_resize_uint8_linear(
        srcImage.data, srcImage.width, srcImage.height, 0, dstImage.data,
        dstImage.width, dstImage.height, 0,
        static_cast<stbir_pixel_layout>(dstImage.channelCount));
}

} // namespace Fly
