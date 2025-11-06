#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_16bit_storage : require
#include "bindless.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outView;
layout(location = 2) out float outRoughness;
layout(location = 3) out float outMetallic;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint vertexBufferIndex;
    uint remapBufferIndex;
    uint instanceBufferIndex;
    uint radianceProjectionBufferIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
    float near;
    float far;
    float halfTanFovHorizontal;
    float halfTanFovVertical;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    f16vec3 position;
    float16_t pad;
    uint normal;
    uint pad1;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, MeshInstance, {
    vec3 position;
    float roughness;
    float metallic;
    float pad[3];
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Remap, { uint value; })

vec3 DecodeNormal(uint quantized)
{
    const float scale = 1.0 / 1023.0; // 10-bit max value is 1023

    uint xBits = (quantized >> 20) & 0x3FFu;
    uint yBits = (quantized >> 10) & 0x3FFu;
    uint zBits = quantized & 0x3FFu;

    vec3 normal;
    normal.x = float(xBits) * scale;
    normal.y = float(yBits) * scale;
    normal.z = float(zBits) * scale;

    normal = normal * 2.0f - 1.0f;

    return normalize(normal);
}

void main()
{
    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, view);

    mat3 R = mat3(view);
    vec3 T = vec3(view[3]);
    vec3 camPos = -transpose(R) * T;

    Vertex v = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gPushConstants.vertexBufferIndex)[gl_VertexIndex];
    vec3 position = vec3(v.position);

    uint instanceIndex =
        FLY_ACCESS_STORAGE_BUFFER(
            Remap, gPushConstants.remapBufferIndex)[gl_InstanceIndex]
            .value;
    MeshInstance instance = FLY_ACCESS_STORAGE_BUFFER(
        MeshInstance, gPushConstants.instanceBufferIndex)[instanceIndex];

    outNormal = DecodeNormal(v.normal);
    outView = normalize(camPos - (position + instance.position));
    outRoughness = instance.roughness;
    outMetallic = instance.metallic;

    gl_Position = projection * view * vec4(position + instance.position, 1.0f);
}
