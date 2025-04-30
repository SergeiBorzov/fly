#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;
layout(location = 1) out flat uint outMaterialIndex;

layout(push_constant) uniform Indices
{
    uint cameraIndex;
    uint materialBufferIndex;
    uint indirectDrawBufferIndex;
}
gIndices;

HLS_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

HLS_REGISTER_STORAGE_BUFFER(Vertex, {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
})

HLS_REGISTER_STORAGE_BUFFER(DrawCommand, {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint vertexBufferIndex;
    uint materialIndex;
})

void main()
{
    DrawCommand drawCommand = HLS_ACCESS_STORAGE_BUFFER(DrawCommand, gIndices.indirectDrawBufferIndex)[gl_DrawID];
    Vertex v = HLS_ACCESS_STORAGE_BUFFER(
        Vertex, drawCommand.vertexBufferIndex)[gl_VertexIndex];
    outUV = vec2(v.uvX, v.uvY);
    outMaterialIndex = drawCommand.materialIndex;
    gl_Position =
        HLS_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, projection) *
        HLS_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, view) *
        vec4(v.position, 1.0f);
}
