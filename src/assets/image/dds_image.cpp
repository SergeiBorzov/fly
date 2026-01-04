#include <stdio.h>

#include "core/memory.h"

#include "dds_image.h"
#include "image.h"

// Note: "DDS " string stored in u32

#define NAME_TO_CODE(char0, char1, char2, char3)                               \
    (static_cast<u32>(char0) | static_cast<u32>(char1 << 8) |                  \
     static_cast<u32>(char2 << 16) | static_cast<u32>(char3 << 24))

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define DDS_MAGIC_NUMBER NAME_TO_CODE('D', 'D', 'S', ' ')
#define DX10_MAGIC_NUMBER NAME_TO_CODE('D', 'X', '1', '0')

enum ResourceMisc : u32
{
    DDS_RESOURCE_MISC_TEXTURECUBE = 0x4,
};

enum ResourceMisc2 : u32
{
    DDS_ALPHA_MODE_UNKNOWN = 0x0,
    DDS_ALPHA_MODE_STRAIGHT = 0x1,
    DDS_ALPHA_MODE_PREMULTIPLIED = 0x2,
    DDS_ALPHA_MODE_OPAQUE = 0x3,
    DDS_ALPHA_MODE_CUSTOM = 0x4,
};

enum DxgiFormat : u32
{
    DXGI_FORMAT_UNKNOWN = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
    DXGI_FORMAT_R32G32B32A32_UINT = 3,
    DXGI_FORMAT_R32G32B32A32_SINT = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS = 5,
    DXGI_FORMAT_R32G32B32_FLOAT = 6,
    DXGI_FORMAT_R32G32B32_UINT = 7,
    DXGI_FORMAT_R32G32B32_SINT = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM = 11,
    DXGI_FORMAT_R16G16B16A16_UINT = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM = 13,
    DXGI_FORMAT_R16G16B16A16_SINT = 14,
    DXGI_FORMAT_R32G32_TYPELESS = 15,
    DXGI_FORMAT_R32G32_FLOAT = 16,
    DXGI_FORMAT_R32G32_UINT = 17,
    DXGI_FORMAT_R32G32_SINT = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM = 24,
    DXGI_FORMAT_R10G10B10A2_UINT = 25,
    DXGI_FORMAT_R11G11B10_FLOAT = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_FORMAT_R8G8B8A8_UINT = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM = 31,
    DXGI_FORMAT_R8G8B8A8_SINT = 32,
    DXGI_FORMAT_R16G16_TYPELESS = 33,
    DXGI_FORMAT_R16G16_FLOAT = 34,
    DXGI_FORMAT_R16G16_UNORM = 35,
    DXGI_FORMAT_R16G16_UINT = 36,
    DXGI_FORMAT_R16G16_SNORM = 37,
    DXGI_FORMAT_R16G16_SINT = 38,
    DXGI_FORMAT_R32_TYPELESS = 39,
    DXGI_FORMAT_D32_FLOAT = 40,
    DXGI_FORMAT_R32_FLOAT = 41,
    DXGI_FORMAT_R32_UINT = 42,
    DXGI_FORMAT_R32_SINT = 43,
    DXGI_FORMAT_R24G8_TYPELESS = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
    DXGI_FORMAT_R8G8_TYPELESS = 48,
    DXGI_FORMAT_R8G8_UNORM = 49,
    DXGI_FORMAT_R8G8_UINT = 50,
    DXGI_FORMAT_R8G8_SNORM = 51,
    DXGI_FORMAT_R8G8_SINT = 52,
    DXGI_FORMAT_R16_TYPELESS = 53,
    DXGI_FORMAT_R16_FLOAT = 54,
    DXGI_FORMAT_D16_UNORM = 55,
    DXGI_FORMAT_R16_UNORM = 56,
    DXGI_FORMAT_R16_UINT = 57,
    DXGI_FORMAT_R16_SNORM = 58,
    DXGI_FORMAT_R16_SINT = 59,
    DXGI_FORMAT_R8_TYPELESS = 60,
    DXGI_FORMAT_R8_UNORM = 61,
    DXGI_FORMAT_R8_UINT = 62,
    DXGI_FORMAT_R8_SNORM = 63,
    DXGI_FORMAT_R8_SINT = 64,
    DXGI_FORMAT_A8_UNORM = 65,
    DXGI_FORMAT_R1_UNORM = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
    DXGI_FORMAT_BC1_TYPELESS = 70,
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_TYPELESS = 73,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_TYPELESS = 76,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
    DXGI_FORMAT_BC4_TYPELESS = 79,
    DXGI_FORMAT_BC4_UNORM = 80,
    DXGI_FORMAT_BC4_SNORM = 81,
    DXGI_FORMAT_BC5_TYPELESS = 82,
    DXGI_FORMAT_BC5_UNORM = 83,
    DXGI_FORMAT_BC5_SNORM = 84,
    DXGI_FORMAT_B5G6R5_UNORM = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
    DXGI_FORMAT_BC6H_TYPELESS = 94,
    DXGI_FORMAT_BC6H_UF16 = 95,
    DXGI_FORMAT_BC6H_SF16 = 96,
    DXGI_FORMAT_BC7_TYPELESS = 97,
    DXGI_FORMAT_BC7_UNORM = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB = 99,
    DXGI_FORMAT_AYUV = 100,
    DXGI_FORMAT_Y410 = 101,
    DXGI_FORMAT_Y416 = 102,
    DXGI_FORMAT_NV12 = 103,
    DXGI_FORMAT_P010 = 104,
    DXGI_FORMAT_P016 = 105,
    DXGI_FORMAT_420_OPAQUE = 106,
    DXGI_FORMAT_YUY2 = 107,
    DXGI_FORMAT_Y210 = 108,
    DXGI_FORMAT_Y216 = 109,
    DXGI_FORMAT_NV11 = 110,
    DXGI_FORMAT_AI44 = 111,
    DXGI_FORMAT_IA44 = 112,
    DXGI_FORMAT_P8 = 113,
    DXGI_FORMAT_A8P8 = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM = 115,
    DXGI_FORMAT_P208 = 130,
    DXGI_FORMAT_V208 = 131,
    DXGI_FORMAT_V408 = 132,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE = 189,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE = 190,
    DXGI_FORMAT_FORCE_UINT = 0xffffffff
};

enum Caps : u32
{
    DDSCAPS_COMPLEX = 0x8,
    DDSCAPS_MIPMAP = 0x400000,
    DDSCAPS_TEXTURE = 0x1000
};

enum Caps2 : u32
{
    DDSCAPS2_CUBEMAP = 0x200,
    DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,
    DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,
    DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000,
    DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,
    DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,
    DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,
    DDSCAPS2_VOLUME = 0x200000
};

enum ResourceDimension : u32
{
    D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
    D3D10_RESOURCE_DIMENSION_BUFFER = 1,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
};

enum HeaderFlags : u32
{
    DDSD_CAPS = 0x1,
    DDSD_HEIGHT = 0x2,
    DDSD_WIDTH = 0x4,
    DDSD_PITCH = 0x8,
    DDSD_PIXELFORMAT = 0x1000,
    DDSD_MIPMAPCOUNT = 0x20000,
    DDSD_LINEARSIZE = 0x80000,
    DDSD_DEPTH = 0x800000
};

enum PixelFormatFlags : u32
{
    DDPF_ALPHAPIXELS = 0x1,
    DDPF_ALPHA = 0x2,
    DDPF_FOURCC = 0x4,
    DDPF_RGB = 0x40,
    DDPF_YUV = 0x200,
    DDPF_LUMINANCE = 0x20000
};

namespace Fly
{
namespace DDS
{

static u8 GetChannelCount(u32 fmt)
{
    switch (fmt)
    {
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_BC4_UNORM:
        {
            return 1;
        }

        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_BC5_UNORM:

        {
            return 2;
        }

        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC6H_UF16:
        {
            return 3;
        }

        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        {
            return 4;
        }

        // Not handling all other formats
        default:
        {
            return 0;
        }
    }
}

static ImageStorageType GetImageStorageType(u32 fmt)
{
    switch (fmt)
    {
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        {
            return ImageStorageType::Float;
        }
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        {
            return ImageStorageType::Half;
        }
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        {
            return ImageStorageType::Byte;
        }

        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        {
            return ImageStorageType::BC1;
        }

        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        {
            return ImageStorageType::BC3;
        }

        case DXGI_FORMAT_BC4_UNORM:
        {
            return ImageStorageType::BC4;
        }

        case DXGI_FORMAT_BC5_UNORM:
        {
            return ImageStorageType::BC5;
        }

        case DXGI_FORMAT_BC6H_UF16:
        {
            return ImageStorageType::BC6;
        }

        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        {
            return ImageStorageType::BC7;
        }

        // Not handling all other formats
        default:
        {
            return ImageStorageType::Invalid;
        }
    }
}

bool ReadDXT10(FILE* file, const Header& header, Image& image)
{
    HeaderDXT10 dxt10{};
    if (fread(&dxt10, 1, sizeof(HeaderDXT10), file) != sizeof(HeaderDXT10))
    {
        return false;
    }

    image.storageType = GetImageStorageType(dxt10.dxgiFormat);
    if (image.storageType == ImageStorageType::Invalid)
    {
        return false;
    }

    image.channelCount = GetChannelCount(dxt10.dxgiFormat);
    if (image.channelCount == 0)
    {
        return false;
    }

    image.width = header.width;
    image.height = header.height;
    image.mipCount = header.mipMapCount;
    image.layerCount = dxt10.arraySize;

    u64 imageSize = GetImageSize(image);
    image.data = static_cast<u8*>(Alloc(imageSize));

    u64 offset = 0;
    for (u32 i = 0; i < image.mipCount; i++)
    {
        u32 mipWidth = MAX(image.width >> i, 1);
        u32 mipHeight = MAX(image.height >> i, 1);
        u64 mipSize = GetImageLayerSize(mipWidth, mipHeight, image.channelCount,
                                        image.storageType) *
                      image.layerCount;
        if (fread(image.data + offset, 1, mipSize, file) != mipSize)
        {
            Free(image.data);
            return false;
        }
        offset += mipSize;
    }

    return true;
}

bool LoadDDSImage(String8 path, Image& image)
{
    FILE* file = fopen(path.Data(), "rb");
    Header header{};

    const PixelFormat& pixelFormat = header.pixelFormat;
    bool hasDXT10 = false;
    u32 magicNumber = 0;

    if (!file)
    {
        goto failure;
    }

    if (fread(&magicNumber, 1, sizeof(u32), file) != sizeof(u32))
    {
        goto failure;
    }
    if (magicNumber != DDS_MAGIC_NUMBER)
    {
        goto failure;
    }

    if (fread(&header, 1, sizeof(Header), file) != sizeof(Header))
    {
        goto failure;
    }

    hasDXT10 = (pixelFormat.flags & DDPF_FOURCC) &&
               pixelFormat.fourCC == DX10_MAGIC_NUMBER;
    if (!hasDXT10)
    {
        fprintf(stderr, "Cannot handle specified pixel format\n");
        goto failure;
    }

    if (!ReadDXT10(file, header, image))
    {
        goto failure;
    }

    fclose(file);
    return true;

failure:
    if (file)
    {
        fclose(file);
    }
    return false;
}

} // namespace DDS
} // namespace Fly
