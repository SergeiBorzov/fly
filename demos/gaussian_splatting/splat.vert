#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint splatBufferIndex;
}
gPushConstants;

HLS_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
    float hTanX;
    float hTanY;
    float near;
    float far;
})

HLS_REGISTER_STORAGE_BUFFER(readonly, Splat, {
    vec3 position;
    float r;
    vec3 scale;
    float g;
    float b;
    float a;
})

void main()
{
    mat4 projection = HLS_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, projection);
    mat4 view = HLS_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, view);

    Splat splat = HLS_ACCESS_STORAGE_BUFFER(
        Splat, gPushConstants.splatBufferIndex)[gl_InstanceIndex];

    outColor = vec4(splat.r, splat.g, splat.b, splat.a);
    gl_Position = projection * view * vec4(splat.position, 1.0f);
    gl_PointSize = 1.0;
}
