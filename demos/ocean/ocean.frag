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
    float height = 0.0f;
    float scale = 1.0f;
    vec2 slope = vec2(0.0f);
    vec2 displacement = vec2(0.0f);
    for (int i = 0; i < 4; i++)
    {
        float h = texture(FLY_ACCESS_TEXTURE_BUFFER(
                              Textures, gPushConstants.heightMapCascades[i]),
                          inUV * scale)
                      .r;
        vec4 value =
            texture(FLY_ACCESS_TEXTURE_BUFFER(
                        Textures, gPushConstants.diffDisplacementCascades[i]),
                    inUV * scale);

        slope += value.xy;
        height += h;
        scale *= 4;
    }

    vec3 n = normalize(vec3(-slope.x, 1.0f, -slope.y));
    vec3 v = normalize(inView);
    float fresnel = pow(1.0 - max(0.15, dot(n, v)), 5.0);

    outColor = vec4(vec3(fresnel), 1.0f);
}
