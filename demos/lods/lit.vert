#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec3 outNormal;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint vertexBufferIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec3 position;
    float pad0;
    vec3 normal;
    float pad1;
})

void main()
{
    Vertex v = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gPushConstants.vertexBufferIndex)[gl_VertexIndex];
    outNormal = v.normal;
    gl_Position = FLY_ACCESS_UNIFORM_BUFFER(
                      Camera, gPushConstants.cameraBufferIndex, projection) *
                  FLY_ACCESS_UNIFORM_BUFFER(
                      Camera, gPushConstants.cameraBufferIndex, view) *
                  vec4(v.position, 1.0f);
}
