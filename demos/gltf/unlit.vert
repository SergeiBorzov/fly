#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform Indices
{
    uint cameraIndex;
    uint materialBufferIndex;
    uint vertexBufferIndex;
    uint materialIndex;
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

void main()
{
    Vertex v = HLS_ACCESS_STORAGE_BUFFER(
        Vertex, gIndices.vertexBufferIndex)[gl_VertexIndex];
    outUV = vec2(v.uvX, v.uvY);
    gl_Position =
        HLS_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, projection) *
        HLS_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraIndex, view) *
        vec4(v.position, 1.0f);
}
