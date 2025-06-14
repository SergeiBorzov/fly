#version 450

#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outView;
layout(location = 2) out vec3 outNormal;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint diffDisplacementMapIndex;
    uint heightMapIndex;
    uint vertexBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(UniformData, {
    mat4 projection;
    mat4 view;
    vec4 fetchSpeedDirSpread;
    vec4 domainTimeLambdaScale;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec2 position;
    vec2 uv;
})

FLY_REGISTER_TEXTURE_BUFFER(Texture, sampler2D)

const vec2 positions[6] =
    vec2[](vec2(-1.0f, 1.0f), vec2(1.0f, 1.0f), vec2(-1.0f, -1.0f),
           vec2(-1.0f, -1.0f), vec2(1.0f, 1.0f), vec2(1.0f, -1.0f));
const vec2 uvs[6] =
    vec2[](vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f),
           vec2(0.0f, 1.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f));

void main()
{
    Vertex vertex = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gPushConstants.vertexBufferIndex)[gl_VertexIndex];
    outUV = vertex.uv;

    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, view);
    vec4 domainTimeLambdaScale = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, domainTimeLambdaScale);

    vec4 value = texture(FLY_ACCESS_TEXTURE_BUFFER(
                             Texture, gPushConstants.diffDisplacementMapIndex),
                         outUV);
    float height = texture(FLY_ACCESS_TEXTURE_BUFFER(
                               Texture, gPushConstants.heightMapIndex),
                           outUV)
                       .r;
    outNormal = normalize(vec3(-value.x, 1.0f, -value.y));
    vec2 displacement = domainTimeLambdaScale.z * value.zw;

    mat3 R = mat3(view);
    vec3 T = vec3(view[3]);

    vec3 camPos = -transpose(R) * T;
    vec3 worldPos = vec3(vertex.position.x + displacement.x, height,
                         vertex.position.y + displacement.y);
    outView = normalize(camPos - worldPos);

    gl_Position = projection * view * vec4(worldPos, 1.0f);
}
