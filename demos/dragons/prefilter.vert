#version 460
#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;

FLY_REGISTER_UNIFORM_BUFFER(CameraData, {
    mat4 projection;
    mat4 view;
    vec4 screenSize;
    float near;
    float far;
    float halfTanFovHorizontal;
    float halfTanFovVertical;
})

layout(push_constant) uniform PushConstants { uint environmentMapIndex; }
gPushConstants;

const vec2 positions[6] =
    vec2[](vec2(-1.0f, 1.0f), vec2(1.0f, 1.0f), vec2(-1.0f, -1.0f),
           vec2(-1.0f, -1.0f), vec2(1.0f, 1.0f), vec2(1.0f, -1.0f));

const vec2 uvs[6] =
    vec2[](vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f),
           vec2(0.0f, 1.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f));

void main()
{
    outUV = uvs[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0.0f, 1.0f);
}
