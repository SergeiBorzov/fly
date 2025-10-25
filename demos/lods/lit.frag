#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint vertexBufferIndex;
}
gPushConstants;

void main()
{
    vec3 l = vec3(0.0f, 1.0f, 0.0f);
    vec3 n = normalize(inNormal);

    vec3 finalColor = vec3(1.0f)*max(dot(l, n), 0.0f);
    outColor = vec4(finalColor, 1.0f);
}
