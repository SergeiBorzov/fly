#ifndef FLY_TRANSFORM_IMAGE_H
#define FLY_TRANSFORM_IMAGE_H

namespace Fly
{
    struct Image;

    bool ResizeImageSRGB(const Image& srcImage, Image& dstImage);
    bool ResizeImageLinear(const Image& srcImage, Image& dstImage);
}

#endif /* End of FLY_TRANSFORM_IMAGE_H */
