#include <stdio.h>
#include <stdlib.h>

#include "core/memory.h"
#include "core/string8.h"
#include "core/thread_context.h"
#include "core/types.h"
#include "math/functions.h"
#include "math/vec.h"

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

static void SpecularGlossinessToRoughnessMetallicFile(cgltf_data* data)
{
    for (u32 i = 0; i < data->materials_count; i++)
    {
        cgltf_material& material = data->materials[i];
        if (!material.has_pbr_specular_glossiness)
        {
            continue;
        }

        const cgltf_pbr_specular_glossiness& pbrSg =
            material.pbr_specular_glossiness;

        SpecularGlossiness sg{};
        sg.diffuse =
            Math::Vec3(pbrSg.diffuse_factor[0], pbrSg.diffuse_factor[1],
                       pbrSg.diffuse_factor[2]);
        sg.specular =
            Math::Vec3(pbrSg.specular_factor[0], pbrSg.specular_factor[1],
                       pbrSg.specular_factor[2]);
        sg.opacity = pbrSg.diffuse_factor[3];
        sg.glossiness = pbrSg.glossiness_factor;

        MetallicRoughness mr = SpecularGlossinessToMetallicRoughness(sg);

        material.has_pbr_specular_glossiness = false;
        material.has_pbr_metallic_roughness = true;

        cgltf_pbr_metallic_roughness& pbrMr = material.pbr_metallic_roughness;
        pbrMr.base_color_texture = {};
        pbrMr.metallic_roughness_texture = {};

        memcpy(pbrMr.base_color_factor, mr.baseColor.data, sizeof(f32) * 3);
        pbrMr.base_color_factor[3] = mr.opacity;
        pbrMr.metallic_factor = mr.metallic;
        pbrMr.roughness_factor = mr.roughness;
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

    SpecularGlossinessToRoughnessMetallicFile(data);

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
