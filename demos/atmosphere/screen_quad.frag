#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants { uint transmittanceMapIndex; }
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

void main()
{
    vec3 transmittance =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Textures,
                                          gPushConstants.transmittanceMapIndex),
                inUV)
            .rgb;

    outFragColor = vec4(transmittance, 1.0f);
}
