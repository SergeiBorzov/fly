#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint heightMapIndex;
    uint vertexBufferIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Texture, sampler2D)

void main()
{
    vec3 l = normalize(vec3(1.0f, 1.0f, 1.0f));
    vec3 n = normalize(inNormal);
    float gterm = max(dot(l, n), 0.0f);

    vec3 color = vec3(0.0f, 0.0f, 1.0f) * gterm + vec3(0.05f);
    outFragColor = vec4(color, 1.0f);
}
