#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "bindless.glsl"
#include "common.glsl"

layout(location = 0) rayPayloadInEXT Payload payload;

FLY_REGISTER_STORAGE_BUFFER(readonly, SphereData, {
    vec3 center;
    float radius;
    vec3 albedo;
    float reflectionCoeff;
})

void main()
{
    SphereData sphereData = FLY_ACCESS_STORAGE_BUFFER(
        SphereData,
        gPushConstants.sphereBufferIndex)[gl_InstanceCustomIndexEXT];

    payload.depth += 1;
    if (payload.depth > MAX_BOUNCES)
    {
        payload.done = 1;
        return;
    }

    payload.p = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    payload.n = normalize(payload.p - sphereData.center);
    payload.reflectionCoeff = sphereData.reflectionCoeff;
    payload.done = 0;
    payload.throughput *= sphereData.albedo;
}
