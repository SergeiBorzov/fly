#include <stdio.h>
#include <stdlib.h>

#include "core/memory.h"
#include "core/string8.h"
#include "core/thread_context.h"
#include "core/types.h"
#include "math/functions.h"
#include "math/vec.h"

#include "assets/image/export_image.h"
#include "assets/image/image.h"
#include "assets/image/import_image.h"
#include "assets/image/transform_image.h"

#define CGLTF_MALLOC(size) (Fly::Alloc(size))
#define CGLTF_FREE(size) (Fly::Free(size))
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include <cgltf_write.h>

#define R_BRIGHTNESS_COEFF 0.299f
#define G_BRIGHTNESS_COEFF 0.587f
#define B_BRIGHTNESS_COEFF 0.114f

#define DIELECTRIC_SPECULAR 0.04f

using namespace Fly;

template <typename T>
struct DynamicArray
{
public:
    inline u64 Count() const { return count; }
    inline u64 Capacity() const { return capacity; }

    DynamicArray()
    {
        count = 0;
        capacity = 0;
        data = nullptr;
    }

    void Add(const T& value)
    {
        if (count + 1 > capacity)
        {
            if (capacity == 0)
            {
                capacity = 1;
            }
            else
            {
                capacity *= 2;
            }
            data = static_cast<T*>(Realloc(data, sizeof(T) * capacity));
        }
        data[count++] = value;
    }

    void Clear() { count = 0; }

    T* Last()
    {
        if (!data || count == 0)
        {
            return nullptr;
        }
        return &data[count - 1];
    }

    const T* Last() const
    {
        if (!data || count == 0)
        {
            return nullptr;
        }
        return &data[count - 1];
    }

    inline T* Data() { return data; }
    inline const T* Data() const { return data; }
    inline T& operator[](u64 index) { return data[index]; }
    inline const T& operator[](u64 index) const { return data[index]; }

    ~DynamicArray()
    {
        if (data)
        {
            Free(data);
            count = 0;
            capacity = 0;
            data = nullptr;
        }
    }

private:
    u64 count;
    u64 capacity;
    T* data;
};

// Note: math from https://github.com/microsoft/glTF-SDK
struct SpecularGlossiness
{
    Math::Vec3 diffuse;
    Math::Vec3 specular;
    f32 opacity;
    f32 glossiness;
};

struct MetallicRoughness
{
    Math::Vec3 baseColor;
    f32 metallic;
    f32 roughness;
    f32 opacity;
};

static f32 Brightness(Math::Vec3 color)
{
    const Math::Vec3 brightness =
        Math::Vec3(R_BRIGHTNESS_COEFF, G_BRIGHTNESS_COEFF, B_BRIGHTNESS_COEFF) *
        (color * color);
    return Math::Sqrt(brightness.x + brightness.y + brightness.z);
}

static f32 SolveMetallic(f32 diffuse, f32 specular,
                         f32 oneMinusSpecularStrength)
{
    if (specular < DIELECTRIC_SPECULAR)
    {
        return 0.0f;
    }

    const f32 a = DIELECTRIC_SPECULAR;
    const f32 b =
        diffuse * oneMinusSpecularStrength / (1.0f - DIELECTRIC_SPECULAR) +
        specular - 2.0f * DIELECTRIC_SPECULAR;
    const f32 c = DIELECTRIC_SPECULAR - specular;
    const f32 d = b * b - 4.0f * a * c;
    return Math::Clamp((-b + Math::Sqrt(d)) / (2.0f * a), 0.0f, 1.0f);
}

static MetallicRoughness
SpecularGlossinessToMetallicRoughness(const SpecularGlossiness& sg)
{
    const f32 oneMinusSpecularStrength =
        1.0f -
        Math::Max(sg.specular.x, Math::Max(sg.specular.y, sg.specular.z));

    const f32 brightnessDiffuse = Brightness(sg.diffuse);
    const f32 brightnessSpecular = Brightness(sg.specular);

    const f32 metallic = SolveMetallic(brightnessDiffuse, brightnessSpecular,
                                       oneMinusSpecularStrength);
    const f32 oneMinusMetallic = 1.0f - metallic;

    const Math::Vec3 baseColorDiffuse =
        sg.diffuse * (oneMinusSpecularStrength / (1.0f - DIELECTRIC_SPECULAR) /
                      Math::Max(oneMinusMetallic, FLY_MATH_EPSILON));
    const Math::Vec3 baseColorSpecular =
        (sg.specular - (DIELECTRIC_SPECULAR * oneMinusMetallic)) *
        (1.0f / Math::Max(metallic, FLY_MATH_EPSILON));

    const Math::Vec3 baseColor = Math::Clamp(
        Math::Lerp(baseColorDiffuse, baseColorSpecular, metallic * metallic),
        0.0f, 1.0f);

    MetallicRoughness mr{};
    mr.baseColor = baseColor;
    mr.opacity = sg.opacity;
    mr.metallic = metallic;
    mr.roughness = 1.0f - sg.glossiness;

    return mr;
};

static String8 GetImagePath(const cgltf_texture* texture)
{
    if (!texture)
    {
        return String8();
    }

    cgltf_image* cgltfImage = texture->image;
    if (texture->has_basisu)
    {
        cgltfImage = texture->basisu_image;
    }
    if (!cgltfImage || !cgltfImage->uri)
    {
        return String8();
    }

    return String8(cgltfImage->uri, strlen(cgltfImage->uri));
}

Math::Vec4 SampleRGBA(const Image& image, u32 x, u32 y, Math::Vec4 defaultValue)
{
    if (!image.data)
    {
        return defaultValue;
    }
    FLY_ASSERT(image.channelCount == 4);

    const f32 inv = 1.0f / 255.0f;
    const u32 idx = (y * image.width + x) * image.channelCount;

    Math::Vec4 sample(image.data[idx + 0] * inv, image.data[idx + 1] * inv,
                      image.data[idx + 2] * inv, image.data[idx + 3] * inv);

    return sample;
}

void WriteRGBA(const Image& image, u32 x, u32 y, Math::Vec4 value)
{
    if (!image.data)
    {
        return;
    }
    FLY_ASSERT(image.channelCount == 4);

    const f32 scale = 255.0f;
    const u32 idx = (y * image.width + x) * image.channelCount;

    image.data[idx + 0] = static_cast<u8>(value.x * scale);
    image.data[idx + 1] = static_cast<u8>(value.y * scale);
    image.data[idx + 2] = static_cast<u8>(value.z * scale);
    image.data[idx + 3] = static_cast<u8>(value.w * scale);
}

static void SpecularGlossinessToRoughnessMetallicFile(
    cgltf_data* data, DynamicArray<cgltf_image*> addImages,
    DynamicArray<cgltf_texture*> addTextures)
{
    for (u32 i = 0; i < data->materials_count; i++)
    {
        cgltf_material& material = data->materials[i];
        if (!material.has_pbr_specular_glossiness)
        {
            continue;
        }

        const cgltf_pbr_specular_glossiness& gltfSg =
            material.pbr_specular_glossiness;
        SpecularGlossiness sg{};
        {
            memcpy(sg.diffuse.data, gltfSg.diffuse_factor, sizeof(f32) * 3);
            memcpy(sg.specular.data, gltfSg.specular_factor, sizeof(f32) * 3);
            sg.opacity = gltfSg.diffuse_factor[3];
            sg.glossiness = gltfSg.glossiness_factor;
        }

        MetallicRoughness mr = SpecularGlossinessToMetallicRoughness(sg);

        cgltf_pbr_metallic_roughness& gltfMr = material.pbr_metallic_roughness;
        {
            gltfMr.base_color_texture = {};
            gltfMr.metallic_roughness_texture = {};

            memcpy(gltfMr.base_color_factor, mr.baseColor.data,
                   sizeof(f32) * 3);
            gltfMr.base_color_factor[3] = mr.opacity;
            gltfMr.metallic_factor = mr.metallic;
            gltfMr.roughness_factor = mr.roughness;
        }

        material.has_pbr_specular_glossiness = false;
        material.has_pbr_metallic_roughness = true;

        const cgltf_texture* diffuseTexture = gltfSg.diffuse_texture.texture;
        const cgltf_texture* specularTexture =
            gltfSg.specular_glossiness_texture.texture;
        String8 diffusePath = GetImagePath(diffuseTexture);
        String8 specularPath = GetImagePath(specularTexture);

        Image diffuseImage{};
        Image specularImage{};
        bool hasDiffuseImage =
            diffusePath && LoadImageFromFile(diffusePath, diffuseImage, 4);
        bool hasSpecularImage =
            specularPath && LoadImageFromFile(specularPath, specularImage, 4);

        if (!hasDiffuseImage && !hasSpecularImage)
        {
            continue;
        }

        Arena& arena = GetScratchArena();
        ArenaMarker marker = ArenaGetMarker(arena);
        Image baseColorImage{};
        Image roughnessMetallicImage{};
        u32 width = Math::Max(diffuseImage.width, specularImage.width);
        u32 height = Math::Max(diffuseImage.height, specularImage.height);

        if (hasDiffuseImage &&
            (diffuseImage.width != width || diffuseImage.height != height))
        {
            ResizeImageSRGB(width, height, diffuseImage);
        }

        if (hasSpecularImage &&
            (specularImage.width != width || specularImage.height != height))
        {
            ResizeImageLinear(width, height, specularImage);
        }

        {
            baseColorImage.data = FLY_PUSH_ARENA(arena, u8, width * height * 4);
            baseColorImage.width = width;
            baseColorImage.height = height;
            baseColorImage.channelCount = 4;
            baseColorImage.layerCount = 1;
            baseColorImage.mipCount = 1;
            baseColorImage.storageType = ImageStorageType::Byte;

            if (hasSpecularImage)
            {
                roughnessMetallicImage.data =
                    FLY_PUSH_ARENA(arena, u8, width * height * 4);
                roughnessMetallicImage.width = width;
                roughnessMetallicImage.height = height;
                roughnessMetallicImage.channelCount = 4;
                roughnessMetallicImage.layerCount = 1;
                roughnessMetallicImage.mipCount = 1;
                roughnessMetallicImage.storageType = ImageStorageType::Byte;
            }
        }

        for (u32 i = 0; i < height; i++)
        {
            for (u32 j = 0; j < width; j++)
            {
                Math::Vec4 diffuseSample =
                    SampleRGBA(diffuseImage, j, i, Math::Vec4(1.0f));
                Math::Vec4 specularSample =
                    SampleRGBA(specularImage, j, i, Math::Vec4(0.0f));

                SpecularGlossiness tsg{};
                tsg.diffuse = Math::Vec3(diffuseSample);
                tsg.specular = Math::Vec3(specularSample);
                tsg.opacity = diffuseSample.w;
                tsg.glossiness = specularSample.w;

                MetallicRoughness trm =
                    SpecularGlossinessToMetallicRoughness(tsg);

                WriteRGBA(baseColorImage, j, i,
                          Math::Vec4(trm.baseColor, trm.opacity));
                if (hasSpecularImage)
                {
                    WriteRGBA(
                        roughnessMetallicImage, j, i,
                        Math::Vec4(0.0f, trm.roughness, trm.metallic, 1.0f));
                }
            }
        }

        String8List strList{};
        String8Node strNodes[2] = {};
        strList.PushExplicit(&strNodes[0], diffusePath);
        strList.PushExplicit(&strNodes[1], FLY_STRING8_LITERAL("_bc.ktx2"));
        String8 baseColorOutputPath = strList.Join(arena);

        bool res = ExportImage(baseColorOutputPath, baseColorImage);
        if (res)
        {
            fprintf(stdout, "Exported converted base color to %.*s\n",
                    static_cast<i32>(baseColorOutputPath.Size()),
                    baseColorOutputPath.Data());
        }
        else
        {
            fprintf(stderr, "Failed to export %.*s",
                    static_cast<i32>(baseColorOutputPath.Size()),
                    baseColorOutputPath.Data());
        }
        cgltf_image* baseColorImageCgltf =
            static_cast<cgltf_image*>(Alloc(sizeof(cgltf_image)));
        memset(baseColorImageCgltf, 0, sizeof(cgltf_image));
        baseColorImageCgltf->uri =
            static_cast<char*>(Alloc(baseColorOutputPath.Size()));
        memcpy(baseColorImageCgltf->uri, baseColorOutputPath.Data(),
               baseColorOutputPath.Size());

        cgltf_texture* baseColorTexture =
            static_cast<cgltf_texture*>(Alloc(sizeof(cgltf_texture)));
        memset(baseColorTexture, 0, sizeof(cgltf_texture));
        baseColorTexture->image = baseColorImageCgltf;
        addImages.Add(baseColorImageCgltf);
        addTextures.Add(baseColorTexture);
        gltfMr.base_color_texture.texture = baseColorTexture;
        gltfMr.base_color_texture.texcoord = 0;

        if (hasSpecularImage)
        {
            String8List strList{};
            String8Node strNodes[2] = {};
            strList.PushExplicit(&strNodes[0], specularPath);
            strList.PushExplicit(&strNodes[1], FLY_STRING8_LITERAL("_rm.ktx2"));
            String8 rmOutputPath = strList.Join(arena);

            bool res = ExportImage(rmOutputPath, roughnessMetallicImage);
            if (res)
            {
                fprintf(
                    stdout, "Exported converted roughness metallic to %.*s\n",
                    static_cast<i32>(rmOutputPath.Size()), rmOutputPath.Data());
            }
            else
            {
                fprintf(stderr, "Failed to export %.*s",
                        static_cast<i32>(rmOutputPath.Size()),
                        rmOutputPath.Data());
            }

            cgltf_image* rmImageCgltf =
                static_cast<cgltf_image*>(Alloc(sizeof(cgltf_image)));
            memset(rmImageCgltf, 0, sizeof(cgltf_image));
            rmImageCgltf->uri = static_cast<char*>(Alloc(rmOutputPath.Size()));
            memcpy(rmImageCgltf->uri, rmOutputPath.Data(), rmOutputPath.Size());

            cgltf_texture* rmTexture =
                static_cast<cgltf_texture*>(Alloc(sizeof(cgltf_texture)));
            memset(rmTexture, 0, sizeof(cgltf_texture));

            rmTexture->image = rmImageCgltf;
            addImages.Add(rmImageCgltf);
            addTextures.Add(rmTexture);
            gltfMr.metallic_roughness_texture.texture = rmTexture;
            gltfMr.metallic_roughness_texture.texcoord = 0;
        }

        ArenaPopToMarker(arena, marker);
    }
}

int main(int argc, char* argv[])
{
    InitArenas();

    if (argc < 3)
    {
        fprintf(stderr, "Semantic error: Not enough arguments\n");
        return -1;
    }

    if (argc > 3)
    {
        fprintf(stderr, "Semantic warning: Extra arguments ignored\n");
    }

    String8 inputGltf = String8(argv[1], strlen(argv[1]));
    String8 outputGltf = String8(argv[2], strlen(argv[2]));

    if (inputGltf == outputGltf)
    {
        fprintf(stderr, "Semantic error: Output should differ from input\n");
        return -2;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, inputGltf.Data(), &data) !=
        cgltf_result_success)
    {
        fprintf(stderr, "Runtime error: Failed to parse %s gltf file\n",
                inputGltf.Data());
        return -3;
    }

    DynamicArray<cgltf_image*> addImages;
    DynamicArray<cgltf_texture*> addTextures;
    SpecularGlossinessToRoughnessMetallicFile(data, addImages, addTextures);

    cgltf_image* images = static_cast<cgltf_image*>(
        Alloc(sizeof(cgltf_image) * data->images_count + addImages.Count()));
    cgltf_texture* textures = static_cast<cgltf_texture*>(Alloc(
        sizeof(cgltf_texture) * data->textures_count + addTextures.Count()));

    memcpy(images, data->images, data->images_count * sizeof(cgltf_image));
    memcpy(images + data->images_count, addImages.Data(),
           addImages.Count() * sizeof(cgltf_image));
    memcpy(textures, data->textures,
           data->textures_count * sizeof(cgltf_texture));
    memcpy(textures + data->textures_count, addTextures.Data(),
           addTextures.Count() * sizeof(cgltf_texture));

    data->images = images;
    data->images_count += addImages.Count();
    data->textures = textures;
    data->textures_count += addTextures.Count();

    if (cgltf_write_file(&options, outputGltf.Data(), data) !=
        cgltf_result_success)
    {
        fprintf(stderr, "Runtime error: Failed to write %s output gltf file\n",
                outputGltf.Data());
        return -4;
    }

    cgltf_free(data);
    return 0;
}
