#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec3 outDirection;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
    vec4 screenSize;
    float near;
    float far;
    float halfTanFovHorizontal;
    float halfTanFovVertical;
})

const vec2 positions[6] =
    vec2[](vec2(-1.0f, 1.0f), vec2(1.0f, 1.0f), vec2(-1.0f, -1.0f),
           vec2(-1.0f, -1.0f), vec2(1.0f, 1.0f), vec2(1.0f, -1.0f));
const vec2 uvs[6] =
    vec2[](vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f),
           vec2(0.0f, 1.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f));

void main()
{
    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, view);

    vec2 ndc = positions[gl_VertexIndex];
    vec4 rayClip = vec4(ndc, 1.0f, 1.0f);
    vec4 rayView = inverse(projection) * rayClip;
    rayView /= rayView.w;
    mat3 invView = transpose(mat3(view));
    outDirection = normalize(invView * rayView.xyz);

    gl_Position = vec4(ndc, 0.0f, 1.0f);
}
