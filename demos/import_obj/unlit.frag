#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Indices
{
    uint cameraIndex;
    uint vertexBufferIndex;
    uint textureIndex;
} uIndices;

layout(set = 0, binding = 2) uniform sampler2D uDiffuseSamplers[];

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(uDiffuseSamplers[uIndices.textureIndex], inUV);
}
