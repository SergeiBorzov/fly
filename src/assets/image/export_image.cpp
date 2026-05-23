#include <string.h>

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "export_image.h"
#include "image.h"

#define STBIW_ASSERT(x) FLY_ASSERT(x)
#define STBIW_MALLOC(size) (Fly::Alloc(size))
#define STBIW_REALLOC(p, newSize) (Fly::Realloc(p, newSize))
#define STBIW_FREE(p) (Fly::Free(p))
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <ktx.h>
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

static bool ExportKTX2(String8 path, const Image& image)
{
    FLY_ASSERT(image.storageType == ImageStorageType::Byte);

    ktxTextureCreateInfo createInfo = {};
    createInfo.vkFormat = 0; // VK_FORMAT_UNDEFINED;
    switch (image.channelCount)
    {
        case 1:
        {
            createInfo.vkFormat = 9; // VK_FORMAT_R8_UNORM;
            break;
        }
        case 2:
        {
            createInfo.vkFormat = 16; // VK_FORMAT_R8G8_UNORM;
            break;
        }
        case 3:
        {
            createInfo.vkFormat = 23; // VK_FORMAT_R8G8B8_UNORM;
            break;
        }
        case 4:
        {
            createInfo.vkFormat = 37; // VK_FORMAT_R8G8B8A8_UNORM;
            break;
        }
        default:
        {
            return false;
        }
    }

    createInfo.baseWidth = image.width;
    createInfo.baseHeight = image.height;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLayers = image.layerCount;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.numLevels = 1;
    createInfo.generateMipmaps = KTX_FALSE;

    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_Create(
        &createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);

    if (result != KTX_SUCCESS)
    {
        return false;
    }

    const ktx_size_t imageSize =
        image.width * image.height * image.channelCount;
    {
        u8* tmpRow = static_cast<u8*>(Alloc(image.width * image.channelCount));
        for (u32 y = 0; y < image.height / 2; y++)
        {
            u8* rowTop = image.data + y * image.width * image.channelCount;
            u8* rowBottom = image.data + (image.height - 1 - y) * image.width *
                                             image.channelCount;
            memcpy(tmpRow, rowTop, image.width * image.channelCount);
            memcpy(rowTop, rowBottom, image.width * image.channelCount);
            memcpy(rowBottom, tmpRow, image.width * image.channelCount);
        }
        Free(tmpRow);
    }

    result =
        ktxTexture_SetImageFromMemory(reinterpret_cast<ktxTexture*>(texture),
                                      0, // mip level
                                      0, // layer
                                      0, // faceSlice
                                      image.data, imageSize);
    if (result != KTX_SUCCESS)
    {
        ktxTexture2_Destroy(texture);
        return false;
    }

    ktxBasisParams params = {};
    params.structSize = sizeof(params);
    params.etc1sCompressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
    params.qualityLevel = KTX_PACK_ASTC_QUALITY_LEVEL_MEDIUM;
    params.threadCount = 0;

    result = ktxTexture2_CompressBasisEx(texture, &params);

    if (result != KTX_SUCCESS)
    {
        ktxTexture2_Destroy(texture);
        return false;
    }

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);
    const char* pathCStr = String8::PushCStr(scratch, path);
    result = ktxTexture_WriteToNamedFile(reinterpret_cast<ktxTexture*>(texture),
                                         pathCStr);
    ArenaPopToMarker(scratch, marker);

    ktxTexture2_Destroy(texture);
    return result == KTX_SUCCESS;
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
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".png")))
            {
                return ExportPNG(path, image);
            }
            else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".jpg")) ||
                     String8::EndsWith(path, FLY_STRING8_LITERAL(".jpeg")))
            {
                return ExportJPG(path, image);
            }
            else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".bmp")))
            {
                return ExportBMP(path, image);
            }
            else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".tga")))
            {
                return ExportTGA(path, image);
            }
            else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".ktx2")))
            {
                return ExportKTX2(path, image);
            }
            break;
        }
        case ImageStorageType::Half:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".exr")))
            {
                return ExportEXR(path, image);
            }
            break;
        }
        case ImageStorageType::Float:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".exr")))
            {
                return ExportEXR(path, image);
            }
            else if (String8::EndsWith(path, FLY_STRING8_LITERAL(".hdr")))
            {
                return ExportHDR(path, image);
            }
            break;
        }
        case ImageStorageType::BC1:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc1")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC3:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc3")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC4:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc4")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC5:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc5")))
            {
                ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC6:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc6")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        case ImageStorageType::BC7:
        {
            if (String8::EndsWith(path, FLY_STRING8_LITERAL(".fbc7")))
            {
                return ExportCookedImage(path, image);
            }
            break;
        }
        default:
        {
            return false;
        }
    }
    return false;
}

} // namespace Fly
