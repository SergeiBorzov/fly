#ifndef HLS_ASSETS_IMPORT_IMAGE_H
#define HLS_ASSETS_IMPORT_IMAGE_H

namespace Hls
{

struct Device;
struct Texture;

struct Image
{
    u8* data = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 channelCount = 0;
};

bool LoadImageFromFile(const char* filename, Image& image);
void FreeImage(Image& image);

bool TransferImageDataToTexture(Device& device, const Image& image,
                                Texture& texture);

} // namespace Hls

#endif /* HLS_ASSETS_IMPORT_IMAGE_H */
