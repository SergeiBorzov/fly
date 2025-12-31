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

layout(push_constant) uniform PushConstants
{
    mat4 model;
    uint cameraIndex;
    uint vertexBufferIndex;
    uint materialBufferIndex;
    int materialIndex;
}
gPushConstants;

FLY_REGISTER_STORAGE_BUFFER(readonly, PBRMaterial, {
    vec4 baseColor;
    uint baseColorTextureIndex;
    uint normalTextureIndex;
    uint ormTextureIndex;
    uint pad;
})
FLY_REGISTER_TEXTURE_BUFFER(Texture2D, sampler2D)

void main()
{
    PBRMaterial material = FLY_ACCESS_STORAGE_BUFFER(
        PBRMaterial,
        gPushConstants.materialBufferIndex)[gPushConstants.materialIndex];

    vec3 baseColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                 Texture2D, material.baseColorTextureIndex),
                             vIn.uv)
                         .rgb;

    vec3 color = baseColor * material.baseColor.rgb;

    vec3 n = normalize(vIn.normal);
    vec3 l = normalize(vec3(0.2f, 1.0f, 0.3f));
    vec3 luminance = max(dot(l, n), 0.0f) * color;

    outColor = vec4(luminance, 1.0f);
}
