#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_multiview : require
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

vec3 GetViewDirection(vec2 coord, uint faceIndex)
{
    if (faceIndex == 0)
    {
        return normalize(vec3(1.0f, coord.y, -coord.x));
    }
    else if (faceIndex == 1)
    {
        return normalize(vec3(-1.0f, coord.y, coord.x));
    }
    else if (faceIndex == 2)
    {
        return normalize(vec3(coord.x, 1.0f, -coord.y));
    }
    else if (faceIndex == 3)
    {
        return normalize(vec3(coord.x, -1.0f, coord.y));
    }
    else if (faceIndex == 4)
    {
        return normalize(vec3(coord.x, coord.y, 1.0f));
    }
    else
    {
        return normalize(vec3(-coord.x, coord.y, -1.0f));
    }
}

void main()
{
    vec3 dir = GetViewDirection(inUV * 2.0f - 1.0f, gl_ViewIndex);

    vec3 skyBoxColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                   Cubemaps, gPushConstants.skyBoxTextureIndex),
                               dir)
                           .rgb;
    outFragColor = vec4(Reinhard(skyBoxColor, 1.0f), 1.0f);
}
