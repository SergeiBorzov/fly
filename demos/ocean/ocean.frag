#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inView;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint diffDisplacementMapIndex;
    uint heightMapIndex;
    uint vertexBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

void main()
{
    vec3 baseColor = vec3(0.0f, 0.01f, 0.02f);
    vec3 l = normalize(vec3(1.0f, 1.0f, 1.0f));

    vec3 n = normalize(inNormal);
    vec3 v = normalize(inView);
    vec3 h = normalize(v + l);
    vec3 r = reflect(-v, n);

    vec3 reflectedColor =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Cubemaps,
                                          gPushConstants.skyBoxTextureIndex),
                r)
            .rgb;

    float fresnel = pow(1.0 - max(0.0, dot(n, v)), 5.0);
    vec3 color = mix(baseColor, reflectedColor, fresnel);

    outFragColor = vec4(color, 1.0f);
}
