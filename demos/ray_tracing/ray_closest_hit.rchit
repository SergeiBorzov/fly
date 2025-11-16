#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "bindless.glsl"

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint sphereBufferIndex;
    uint accStructureIndex;
    uint outputTextureIndex;
    uint sbtStride;
}
gPushConstants;

FLY_REGISTER_STORAGE_BUFFER(readonly, SphereData, {
    vec3 center;
    float radius;
    vec3 albedo;
    float reflectionCoeff;
})

layout(location = 0) rayPayloadInEXT vec4 payload;

void main()
{
    SphereData sphereData = FLY_ACCESS_STORAGE_BUFFER(
        SphereData,
        gPushConstants.sphereBufferIndex)[gl_InstanceCustomIndexEXT];

    vec3 p = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    vec3 n = normalize(p - sphereData.center);

    vec3 albedo = sphereData.albedo;
    vec3 l = vec3(0.0f, 1.0f, 0.0f);
    vec3 color = max(dot(n, l), 0.0f) * albedo;
    payload = vec4(color, 1.0f);
}
