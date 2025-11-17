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

// float RadicalInverse2(uint n)
// {
//     // Note that digits of radical inverse with base 2 (Van der Corput
//     sequence)
//     // Is just the reverse of the bits
//     n = ((n >> 1) & 0x55555555u) | ((n & 0x55555555u) << 1);
//     n = ((n >> 2) & 0x33333333u) | ((n & 0x33333333u) << 2);
//     n = ((n >> 4) & 0x0F0F0F0Fu) | ((n & 0x0F0F0F0Fu) << 4);
//     n = ((n >> 8) & 0x00FF00FFu) | ((n & 0x00FF00FFu) << 8);
//     n = (n >> 16) | (n << 16);

//     return float(n) * 2.3283064365386963e-10;
// }

// vec2 Hammersley(uint i, uint N)
// {
//     return vec2(float(i) / float(N), RadicalInverse2(i));
// }

uint Xorshift32(inout uint seed)
{
    seed = seed ^ 61u ^ (seed >> 16);
    seed *= 9u;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat(inout uint seed)
{
    return float(Xorshift32(seed)) / 4294967296.0;
}

vec3 SampleHemisphere(vec2 p, vec3 n)
{
    float phi = 2.0f * PI * p.x;
    float cosTheta = sqrt((1.0f - p.y));
    float sinTheta = sqrt(p.y);

    vec3 h;
    h.x = cos(phi) * sinTheta;
    h.y = cosTheta;
    h.z = sin(phi) * sinTheta;

    vec3 up =
        abs(n.y) < 0.999f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);

    vec3 sampleVec = tangent * h.x + n * h.y + bitangent * h.z;
    return normalize(sampleVec);
}

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

    vec3 p = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    vec3 n = normalize(p - sphereData.center);

    payload.newOrigin = p + n * 0.05f;
    payload.newDir = SampleHemisphere(
        vec2(RandomFloat(payload.seed), RandomFloat(payload.seed)), n);
    payload.done = 0;
    payload.throughput *= sphereData.albedo;
}
