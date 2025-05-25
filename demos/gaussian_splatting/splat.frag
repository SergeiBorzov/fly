#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    float l = -dot(inUV, inUV);
    if (l < -4.0)
    {
        discard;
    }
    float alpha = inColor.a * exp(l);
    outColor = vec4(inColor.rgb, alpha);
}
