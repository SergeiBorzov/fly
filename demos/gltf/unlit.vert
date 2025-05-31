#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform Indices
{
    mat4 model;
    uint cameraIndex;
    uint materialBufferIndex;
    uint vertexBufferIndex;
    uint materialIndex;
}
gIndices;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
})

void main()
{
    Vertex v = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gIndices.vertexBufferIndex)[gl_VertexIndex];
    outUV = vec2(v.uvX, v.uvY);
    gl_Position =
        FLY_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, projection) *
        FLY_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, view) * gIndices.model *
        vec4(v.position, 1.0f);
}
