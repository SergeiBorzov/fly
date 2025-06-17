#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inView;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in float inHeight;

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
    vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
    vec3 subsurfaceColor = vec3(0.02, 0.1, 0.15) * 5;
    vec3 baseColor = vec3(0.0f, 0.5f, 0.8f);
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

    float k1 = 0.3f;
    float k2 = 0.1f;
    float k3 = 0.1f;
    float k4 = 0.1f;

    float fresnel = pow(1.0 - max(0.15, dot(n, v)), 5.0);
    float ss1 = k1 * max(0, inHeight) * pow(dot(l, -v), 4.0f) *
                pow(0.5f - 0.5f * (dot(l, n)), 3.0f);
    float ss2 = k2 * (pow(dot(v, n), 2.0f));
    vec3 ss = (1 - fresnel) * (ss1 + ss2) * subsurfaceColor * lightColor;

    vec3 specular = lightColor * pow(max(dot(n, h), 0.0f), 250.0f);

    outFragColor = vec4(ss + specular + fresnel * reflectedColor, 1.0f);
}
