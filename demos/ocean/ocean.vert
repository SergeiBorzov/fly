#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint vertexBufferIndex;
    uint heightMapCascades[4];
    uint heightMapDiffDisplacementMaps[4];
    uint skyBoxTextureIndex;
}
gPushConstants;

const vec2 positions[6] =
    vec2[](vec2(-0.5f, -0.5f), vec2(0.5f, -0.5f), vec2(-0.5f, 0.5f),
           vec2(-0.5f, 0.5f), vec2(0.5f, -0.5f), vec2(0.5f, 0.5f));

FLY_REGISTER_UNIFORM_BUFFER(UniformData, {
    mat4 projection;
    mat4 view;
    vec4 screenSize;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec2 position;
    vec2 uv;
})

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

void main()
{
    Vertex vertex = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gPushConstants.vertexBufferIndex)[gl_VertexIndex];
    outUV = vertex.uv;

    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, view);

    vec2 displacement = vec2(0.0f);

    vec3 worldPos = vec3(vertex.position.x, 0.0f, vertex.position.y);

    gl_Position = projection * view * vec4(worldPos, 1.0f);
}
