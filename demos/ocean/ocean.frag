#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint heightMapIndex;
    uint vertexBufferIndex;
}
gPushConstants;



void main()
{
    outFragColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);
}
