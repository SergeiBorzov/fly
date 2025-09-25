#version 450
#extension GL_EXT_multiview : enable
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(push_constant) uniform PushConstants { uint textureIndex; }
gPushConstants;

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outFragColor;

FLY_REGISTER_TEXTURE_BUFFER(Texture2D, sampler2D)

#define PI 3.14159265359f
#define HALF_PI 1.57079632679f

vec3 GetViewDirection(uint faceIndex, vec2 uv)
{
    vec2 coord = uv * 2.0f - 1.0f;

    if (faceIndex == 0)
    {
        return normalize(vec3(1.0f, -coord.y, -coord.x));
    }
    else if (faceIndex == 1)
    {
        return normalize(vec3(-1.0f, -coord.y, coord.x));
    }
    else if (faceIndex == 2)
    {
        return normalize(vec3(coord.x, -1.0f, -coord.y));
    }
    else if (faceIndex == 3)
    {
        return normalize(vec3(coord.x, 1.0f,  coord.y));
    }
    else if (faceIndex == 4)
    {
        return normalize(vec3(coord.x, -coord.y, 1.0f));
    }
    else
    {
        return normalize(vec3(-coord.x, -coord.y, -1.0f));
    }
}

vec2 SampleEquirectangular(vec3 dir)
{
    float theta = atan(dir.z, dir.x);
    float phi = asin(clamp(dir.y, -1.0f, 1.0f));

    vec2 uv;
    uv.x = (theta + PI) / (2.0f * PI);
    uv.y = (phi + HALF_PI) / PI;
    return uv;
}

void main()
{
    vec3 dir = GetViewDirection(gl_ViewIndex, inUVs);
    vec4 sampledColor = texture(
        FLY_ACCESS_TEXTURE_BUFFER(Texture2D, gPushConstants.textureIndex),
        SampleEquirectangular(dir));
    outFragColor = vec4(sampledColor.rgb, 1.0f);
}
