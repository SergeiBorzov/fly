#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Indices
{
    uint sceneDataIndex;
    uint textureIndex;
} uIndices;

layout(set = 0, binding = 2) uniform sampler2D uTextures[];

layout(location = 0) in vec2 inUVs;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor =
        texture(uTextures[uIndices.textureIndex], inUVs) * vec4(inColor, 1.0f);
}
