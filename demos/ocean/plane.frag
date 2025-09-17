#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inPos;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 fogColor = vec3(0.7f, 0.75f, 0.8f);
    float dist = length(inPos);
    float fogFactor = smoothstep(75.0f, 500.0f, dist);

    vec3 finalColor = mix(vec3(inUV, 0.0f), fogColor, fogFactor);
    outColor = vec4(finalColor, 1.0f);
}
