#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint vertexBufferIndex;
    // uint heightMapCascades[4];
    // uint heightMapDiffDisplacementMaps[4];
    // uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

void main() { outColor = vec4(inUV, 0.0f, 1.0f); }
