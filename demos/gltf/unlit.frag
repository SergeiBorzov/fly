#version 450
#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"

layout(location = 0) in VsOut
{
    vec3 normal;
    vec2 uv;
}
vIn;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Indices
{
    mat4 model;
    uint cameraIndex;
    uint vertexBufferIndex;
    uint materialBufferIndex;
    uint materialIndex;
}
gIndices;

struct TextureProperty
{
    vec2 offset;
    vec2 scale;
    uint textureIndex;
    uint pad;
};

FLY_REGISTER_STORAGE_BUFFER(readonly, MaterialData, { TextureProperty albedo; })
FLY_REGISTER_TEXTURE_BUFFER(AlbedoTexture, sampler2D)

void main()
{
    vec3 n = normalize(vIn.normal);
    vec3 l = normalize(vec3(0.2f, 1.0f, 0.3f));
    vec3 luminance = max(dot(l, n), 0.0f) * vec3(1.0f);

    outColor = vec4(luminance, 1.0f);

    // MaterialData material = FLY_ACCESS_STORAGE_BUFFER(
    //     MaterialData, gIndices.materialBufferIndex)[gIndices.materialIndex];
    // outColor = texture(
    //     FLY_ACCESS_TEXTURE_BUFFER(AlbedoTexture,
    //     material.albedo.textureIndex), inUV);
}
