#ifndef FLY_TRANSFORM_IMAGE_H
#define FLY_TRANSFORM_IMAGE_H

#include "image.h"

namespace Fly
{

namespace RHI
{

struct Device;
struct GraphicsPipeline;

}; // namespace RHI

void CopyImage(const Image& srcImage, Image& dstImage);
bool ResizeImageSRGB(u32 width, u32 height, Image& image);
bool ResizeImageLinear(u32 width, u32 height, Image& image);
bool GenerateMips(Image& image, bool linearResize = false);
bool Eq2Cube(RHI::Device& device, RHI::GraphicsPipeline& eq2cubePipeline,
             Image& image);
bool CompressImage(ImageStorageType codec, Image& image);
void TonemapHalf(Image& image);
void TonemapFloat(Image& image);

} // namespace Fly

#endif /* End of FLY_TRANSFORM_IMAGE_H */
