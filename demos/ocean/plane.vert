#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outPos;

layout(push_constant) uniform Indices
{
    uint cameraBufferIndex;
    uint vertexBufferIndex;
}
gIndices;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec2 position;
    vec2 uv;
})

void main()
{
    Vertex v = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gIndices.vertexBufferIndex)[gl_VertexIndex];
    outUV = vec2(v.uv.x, v.uv.y);

    vec4 viewPos = FLY_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraBufferIndex, view) *
        vec4(v.position.x, 0.0f, v.position.y, 1.0f);
    outPos = viewPos.xyz;
    gl_Position =
        FLY_ACCESS_UNIFORM_BUFFER(Camera, gIndices.cameraBufferIndex,
                                  projection) * viewPos;
}
