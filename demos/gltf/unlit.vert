#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_16bit_storage : require
#include "bindless.glsl"

layout(location = 0) out VsOut
{
    vec3 normal;
    vec2 uv;
}
vOut;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    uint cameraBufferIndex;
    uint vertexBufferIndex;
    uint materialBufferIndex;
    int materialIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    float16_t positionX;
    float16_t positionY;
    float16_t positionZ;
    float16_t u;
    float16_t v;
    uint normal;
    uint tangent;
})

vec3 DecodeVector3(uint quantized)
{
    const float scale = 1.0 / 1023.0;

    uint xBits = (quantized >> 20) & 0x3FFu;
    uint yBits = (quantized >> 10) & 0x3FFu;
    uint zBits = quantized & 0x3FFu;

    vec3 vec;
    vec.x = float(xBits) * scale;
    vec.y = float(yBits) * scale;
    vec.z = float(zBits) * scale;

    vec = vec * 2.0f - 1.0f;

    return normalize(vec);
}

void main()
{
    Vertex vertex = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gPushConstants.vertexBufferIndex)[gl_VertexIndex];
    vOut.normal = DecodeVector3(vertex.normal);
    vOut.uv = vec2(vertex.u, vertex.v);
    vec3 position = vec3(vertex.positionX, vertex.positionY, vertex.positionZ);

    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, view);

    gl_Position =
        projection * view * gPushConstants.model * vec4(position, 1.0f);
}
