#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inView;
layout(location = 3) in vec3 inPos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint vertexBufferIndex;
    uint heightMapCascades[4];
    uint diffDisplacementCascades[4];
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

void main()
{
    // float height = 0.0f;
    // float scale = 1.0f;
    // for (int i = 0; i < 2; i++)
    // {
    //     float h = texture(FLY_ACCESS_TEXTURE_BUFFER(
    //                           Textures, gPushConstants.heightMapCascades[i]),
    //                       inUV * scale)
    //                   .r;
    //     height += h / scale;
    //     scale *= 4;
    // }

    float scale = 1.0f;
    vec2 slope = vec2(0.0f);
    float height = 0.0f;
    float heightL = 0.0f;
    float heightR = 0.0f;
    float heightB = 0.0f;
    float heightT = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        float hL = texture(FLY_ACCESS_TEXTURE_BUFFER(
                               Textures, gPushConstants.heightMapCascades[i]),
                           (inUV - vec2(1.0f / 256.0f, 0.0f)) * scale)
                       .r;
        float hR = texture(FLY_ACCESS_TEXTURE_BUFFER(
                               Textures, gPushConstants.heightMapCascades[i]),
                           (inUV + vec2(1.0f / 256.0f, 0.0f)) * scale)
                       .r;
        float hB = texture(FLY_ACCESS_TEXTURE_BUFFER(
                               Textures, gPushConstants.heightMapCascades[i]),
                           (inUV - vec2(0.0f, 1.0f / 256.0f)) * scale)
                       .r;
        float hT = texture(FLY_ACCESS_TEXTURE_BUFFER(
                               Textures, gPushConstants.heightMapCascades[i]),
                           (inUV + vec2(0.0f, 1.0f / 256.0f)) * scale)
                       .r;

        vec4 value =
            texture(FLY_ACCESS_TEXTURE_BUFFER(
                        Textures, gPushConstants.diffDisplacementCascades[i]),
                    inUV * scale);
        slope += value.xy / scale;
        heightL += hL / scale;
        heightR += hR / scale;
        heightB += hB / scale;
        heightT += hT / scale;
        scale *= 4;
    }

    // vec3 dpdx = dFdx(inPos);
    // vec3 dpdy = dFdy(inPos);
    // vec3 normal = normalize(cross(dpdx, dpdy));

    // vec3 n = normalize(inNormal);
    vec3 n = normalize(vec3(-(heightR - heightL) / (2.0f / 256.0f), 1.0f,
                            -(heightT - heightB) / (2.0f / 256.0f)));
    //vec3 n = normalize(vec3(-slope.x, 1.0f, -slope.y));
    vec3 v = normalize(inView);

    float fresnel = pow(1.0 - max(0.15, dot(n, v)), 5.0);

    outColor = vec4(n, 1.0f);
}
