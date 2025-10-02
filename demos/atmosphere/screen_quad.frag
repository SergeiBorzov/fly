#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants { uint mapIndex; }
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

// const mat3 ACESInputMat = mat3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834,
//                                0.01566, 0.02840, 0.13383, 0.83777);

// const mat3 ACESOutputMat = mat3(1.60475, -0.53108, -0.07367,
// -0.10208, 1.10813,
//                                 -0.00605, -0.00327, -0.07276, 1.07602);

vec3 RRTAndODTFit(in vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

void main()
{
    vec3 color =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Textures, gPushConstants.mapIndex),
                inUV)
            .rgb;

    // color *= ACESInputMat;
    // color = RRTAndODTFit(color);
    // color *= ACESOutputMat;
    // outFragColor = vec4(transmittance, 1.0f);
    // outFragColor = vec4(color, 1.0f);
    outFragColor = vec4(Reinhard(color, 1.0f), 1.0f);
}
