#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inView;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint heightMapIndex;
    uint vertexBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Texture, sampler2D)
FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

vec3 FresnelSchlick(vec3 f0, vec3 view, vec3 normal)
{
    return f0 + (1.0 - f0) * pow(1.0 - max(dot(view, normal), 0.0), 5.0);
}

void main()
{
    vec3 baseColor = vec3(0.0f, 0.1f, 0.2f);
    vec3 l = normalize(vec3(1.0f, 1.0f, 1.0f));

    vec4 value = texture(
        FLY_ACCESS_TEXTURE_BUFFER(Texture, gPushConstants.heightMapIndex),
        inUV);

    vec3 n = normalize(vec3(-value.y, 1.0f, -value.z));
    vec3 v = normalize(inView);
    vec3 h = normalize(v + l);
    vec3 r = reflect(-v, n);

    vec3 reflectedColor =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Cubemaps,
                                          gPushConstants.skyBoxTextureIndex),
                r)
            .rgb;

    float lambert = max(dot(l, n), 0.0f);

    vec3 fs = FresnelSchlick(vec3(0.04), v, n);

    vec3 color = clamp(baseColor * (1 - fs) + fs * reflectedColor, 0.0f, 1.0f);

    outFragColor = vec4(color, 1.0f);
}
