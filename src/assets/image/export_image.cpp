#include <string.h>

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/memory.h"

#include "export_image.h"
#include "image.h"

#define STBIW_ASSERT(x) FLY_ASSERT(x)
#define STBIW_MALLOC(size) (Fly::Alloc(size))
#define STBIW_REALLOC(p, newSize) (Fly::Realloc(p, newSize))
#define STBIW_FREE(p) (Fly::Free(p))
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <tinyexr.h>

namespace Fly
{

static bool ExportPNG(String8 path, const Image& image)
{
    return stbi_write_png(path.Data(), image.width,
                          image.height * image.layerCount, image.channelCount,
                          image.data,
                          image.width * image.channelCount * sizeof(u8));
}

static bool ExportBMP(String8 path, const Image& image)
{
    return stbi_write_bmp(path.Data(), image.width,
                          image.height * image.layerCount, image.channelCount,
                          image.data);
}

static bool ExportTGA(String8 path, const Image& image)
{
    return stbi_write_tga(path.Data(), image.width,
                          image.height * image.layerCount, image.channelCount,
                          image.data);
}

static bool ExportJPG(String8 path, const Image& image)
{
    return stbi_write_jpg(path.Data(), image.width,
                          image.height * image.layerCount, image.channelCount,
                          image.data, 90);
}

static bool ExportHDR(String8 path, const Image& image)
{
    return stbi_write_hdr(path.Data(), image.width,
                          image.height * image.layerCount, image.channelCount,
                          reinterpret_cast<const float*>(image.data));
}

static bool ExportCookedImage(String8 path, const Image& image)
{
    u64 imageSize = GetImageSize(image);
    u64 totalSize = imageSize + sizeof(ImageHeader);

    u8* data = static_cast<u8*>(Alloc(totalSize));
    ImageHeader* header = reinterpret_cast<ImageHeader*>(data);
    header->size = GetImageSize(image);
    header->offset = sizeof(ImageHeader);
    header->width = image.width;
    header->height = image.height;
    header->channelCount = image.channelCount;
    header->layerCount = image.layerCount;
    header->mipCount = image.mipCount;
    header->storageType = ImageStorageType::Byte;

    u8* imageData = data + sizeof(ImageHeader);
    memcpy(imageData, image.data, imageSize);

    String8 str(reinterpret_cast<const char*>(data), totalSize);
    if (!WriteStringToFile(str, path))
    {
        Free(data);
        return false;
    }

    Free(data);
    return true;
}

static bool ExportEXR(String8 path, const Image& image)
{
    FLY_ASSERT(image.storageType == ImageStorageType::Half);

    EXRHeader exrHeader;
    InitEXRHeader(&exrHeader);

    i32 channelMap[] = {-1, -1, -1, -1};
    {
        exrHeader.num_channels = image.channelCount;
        exrHeader.channels = static_cast<EXRChannelInfo*>(
            Fly::Alloc(sizeof(EXRChannelInfo) * image.channelCount));

        const char* channelNames[] = {"R", "G", "B", "A"};
        for (u32 i = 0; i < image.channelCount; i++)
        {
            strncpy(exrHeader.channels[i].name,
                    channelNames[image.channelCount - i - 1], 255);
            channelMap[i] = image.channelCount - i - 1;
        }

        exrHeader.pixel_types =
            static_cast<int*>(Fly::Alloc(sizeof(int) * image.channelCount));
        exrHeader.requested_pixel_types =
            static_cast<int*>(Fly::Alloc(sizeof(int) * image.channelCount));
        for (int i = 0; i < image.channelCount; i++)
        {
            if (image.storageType == ImageStorageType::Half)
            {
                exrHeader.pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
                exrHeader.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
            }
            else if (image.storageType == ImageStorageType::Float)
            {
                exrHeader.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
                exrHeader.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
            }
        }
    }

    EXRImage exrImage;
    InitEXRImage(&exrImage);

    {
        exrImage.num_channels = image.channelCount;
        exrImage.width = image.width;
        exrImage.height = image.height * image.layerCount;

        f16** images =
            static_cast<f16**>(Fly::Alloc(sizeof(f16*) * image.channelCount));
        for (u32 i = 0; i < image.channelCount; i++)
        {
            images[i] = static_cast<f16*>(Fly::Alloc(
                sizeof(f16) * image.layerCount * image.width * image.height));
        }

        const f16* imageData = reinterpret_cast<const f16*>(image.data);
        for (u32 n = 0; n < image.layerCount; n++)
        {
            for (u32 i = 0; i < image.height; i++)
            {
                for (u32 j = 0; j < image.width; j++)
                {
                    for (u32 k = 0; k < image.channelCount; k++)
                    {
                        images[channelMap[k]][n * image.height * image.width +
                                              i * image.width + j] =
                            imageData[n * image.height * image.width *
                                          image.channelCount +
                                      i * image.width * image.channelCount +
                                      j * image.channelCount + k];
                    }
                }
            }
        }
        exrImage.images = reinterpret_cast<unsigned char**>(images);
    }

    const char* err = nullptr;
    int ret = SaveEXRImageToFile(&exrImage, &exrHeader, path.Data(), &err);

    Free(exrHeader.channels);
    Free(exrHeader.pixel_types);
    Free(exrHeader.requested_pixel_types);

    for (u32 i = 0; i < image.channelCount; i++)
    {
        Free(exrImage.images[i]);
    }
    Free(exrImage.images);

    if (ret != TINYEXR_SUCCESS)
    {
        FreeEXRErrorMessage(err);
        return ret;
    }

    return ret == TINYEXR_SUCCESS;
}

bool ExportImage(String8 path, const Image& image)
{
    switch (image.storageType)
    {
        case ImageStorageType::Byte:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".png")))
            {
                return ExportPNG(path, image);
            }
            else if (path.EndsWith(FLY_STRING8_LITERAL(".jpg")) ||
                     path.EndsWith(FLY_STRING8_LITERAL(".jpeg")))
            {
                return ExportJPG(path, image);
            }
            else if (path.EndsWith(FLY_STRING8_LITERAL(".bmp")))
            {
                return ExportBMP(path, image);
            }
            else if (path.EndsWith(FLY_STRING8_LITERAL(".tga")))
            {
                return ExportTGA(path, image);
            }
            break;
        }
        case ImageStorageType::Half:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".exr")))
            {
                return ExportEXR(path, image);
            }
            break;
        }
        case ImageStorageType::Float:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".exr")))
            {
                return ExportEXR(path, image);
            }
            else if (path.EndsWith(FLY_STRING8_LITERAL(".hdr")))
            {
                return ExportHDR(path, image);
            }
            break;
        }
        case ImageStorageType::BC1:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".fbc1")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC3:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".fbc3")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC4:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".fbc4")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC5:
        {
            if (path.EndsWith(FLY_STRING8_LITERAL(".fbc5")))
            {
                ExportCookedImage(path, image);
            }
            break;
        }
    }
    return false;
}

} // namespace Fly
