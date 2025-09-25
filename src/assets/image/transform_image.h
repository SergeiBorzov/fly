#ifndef FLY_TRANSFORM_IMAGE_H
#define FLY_TRANSFORM_IMAGE_H

namespace Fly
{

struct Image;

namespace RHI
{

struct Device;
struct GraphicsPipeline;

}; // namespace RHI

bool ResizeImageSRGB(const Image& srcImage, u32 width, u32 height,
                     Image& dstImage);
bool ResizeImageLinear(const Image& srcImage, u32 width, u32 height,
                       Image& dstImage);
bool Eq2Cube(RHI::Device& device, RHI::GraphicsPipeline& eq2cubePipeline,
             const Image& srcImage, Image& dstImage, bool generateMips);

} // namespace Fly

#endif /* End of FLY_TRANSFORM_IMAGE_H */
