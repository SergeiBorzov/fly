#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;
layout(location = 1) out flat uint outMaterialIndex;

layout(push_constant) uniform Indices
{
    uint instanceDataBufferIndex;
    uint meshDataBufferIndex;
    uint cameraIndex;
    uint materialBufferIndex;
}
gIndices;

HLS_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

HLS_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
})

HLS_REGISTER_STORAGE_BUFFER(readonly, InstanceData, {
    mat4 model;
    uint meshDataIndex;
    uint pad[3];
})

HLS_REGISTER_STORAGE_BUFFER(readonly, MeshData, {
    uint materialIndex;
    uint vertexBufferIndex;
    uint boundingSphereDrawIndex;
})

void main()
{
    // DrawCommand drawCommand = HLS_ACCESS_STORAGE_BUFFER(
    //     DrawCommand, gIndices.indirectDrawBufferIndex)[gl_DrawID];

    InstanceData instance = HLS_ACCESS_STORAGE_BUFFER(
        InstanceData, gIndices.instanceDataBufferIndex)[gl_InstanceIndex];
    MeshData meshData = HLS_ACCESS_STORAGE_BUFFER(
        MeshData, gIndices.meshDataBufferIndex)[instance.meshDataIndex];
    Vertex v = HLS_ACCESS_STORAGE_BUFFER(
        Vertex, meshData.vertexBufferIndex)[gl_VertexIndex];

    outUV = vec2(v.uvX, v.uvY);
    outMaterialIndex = meshData.materialIndex;
    gl_Position =
        HLS_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, projection) *
        HLS_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, view) *
        instance.model * vec4(v.position, 1.0f);
}
