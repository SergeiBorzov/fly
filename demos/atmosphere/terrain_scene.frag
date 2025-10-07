#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

#define MAX_MARCHING_STEPS 300
#define MAX_DIST 10000.0f
#define EPSILON 0.001f

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint atmosphereBufferIndex;
    uint cameraBufferIndex;
    uint transmittanceMapIndex;
    uint skyviewMapIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

// // https://iquilezles.org/articles/filterableprocedurals/
// float FilteredXor(vec2 p, vec2 dpdx, vec2 dpdy)
// {
//     float xor = 0.0f;
//     for (int i = 0; i < 8; i++)
//     {
//         vec2 w = max(abs(dpdx), abs(dpdy)) + 0.01f;
//         vec2 f = 2.0f *
//                  (abs(fract((p - 0.5f * w) / 2.0f) - 0.5f) -
//                   abs(fract((p + 0.5f * w) / 2.0f) - 0.5f)) /
//                  w;
//         xor+= 0.5f - 0.5f * f.x* f.y;

//         dpdx *= 0.5f;
//         dpdy *= 0.5f;
//         p *= 0.5f;
//         xor*= 0.5f;
//     }
//     return xor;
// }

// float PlaneSdf(vec3 p, vec3 n, float h) { return dot(p, n) - 1; }

// float SceneSdf(vec3 p) { return PlaneSdf(p, vec3(0, 1, 0), 0.0f); }

// int RayMarch(vec3 origin, vec3 dir, float start, float end, out float depth)
// {
//     depth = start;
//     int res = -1;
//     for (int i = 0; i < MAX_MARCHING_STEPS; i++)
//     {
//         float dist = SceneSdf(origin + depth * dir);
//         depth += dist;
//         if (dist < EPSILON * depth)
//         {
//             return 0;
//         }
//         if (depth >= end)
//         {
//             return -1;
//         }
//     }
//     return -1;
// }

vec3 ShadeScene(vec3 origin, vec3 dir, vec3 l, float rb, float rt)
{
    // Sky
    float lat = asin(dir.y);
    float lon = atan(dir.x, dir.z);
    vec3 lum = SampleSkyview(lon, lat, gPushConstants.skyviewMapIndex);

    float sunAngularDiameterRadians = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunAngularDiameterRadians);

    float sunAngularRadius = sunAngularDiameterRadians * 0.5f;
    float r = length(origin);
    float cosZ = dot(l, normalize(origin));

    float k = smoothstep(cos(sunAngularRadius * 1.05f), cos(sunAngularRadius),
                         dot(l, dir));
    float vis = float(RaySphereIntersect(origin, dir, vec3(0.0f), rb) < 0.0f);

    vec3 sunAlbedo = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, sunAlbedo);
    lum += vis * k * sunAlbedo * 16.0f *
           SampleTransmittance(r, cosZ, rb, rt,
                               gPushConstants.transmittanceMapIndex);

    return lum;
}

vec3 RRTAndODTFit(in vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

const mat3 ACESInputMat = mat3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834,
                               0.01566, 0.02840, 0.13383, 0.83777);

const mat3 ACESOutputMat = mat3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813,
                                -0.00605, -0.00327, -0.07276, 1.07602);

vec3 ACES(vec3 color)
{
    color *= ACESInputMat;
    color = RRTAndODTFit(color);
    color *= ACESOutputMat;
    return color;
}

void main()
{
    vec2 uv = inUV;
    uv.y = 1.0 - uv.y;

    float rb = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, rb);
    float rt = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, rt);
    float zenith = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunZenithRadians);
    float azimuth = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunAzimuthRadians);

    vec3 l = SphereCoordToRay(PI / 2 - zenith, azimuth);

    mat4 inverseProjection = inverse(FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, projection));
    mat4 inverseView = inverse(FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, view));

    vec4 clipPos = vec4(2.0f * uv - 1.0f, 0.0f, 1.0f);
    vec4 viewPos = inverseProjection * clipPos;
    viewPos /= viewPos.w;
    vec3 rayVS = normalize(viewPos.xyz);
    vec3 rayWS = normalize(inverseView * vec4(rayVS, 0.0f)).xyz;

    vec3 camPos = inverseView[3].xyz;

    vec3 worldPos = camPos * 0.001f;
    worldPos.y += rb;

    vec3 lum = vec3(0.0f);
    vec3 e = vec3(1.0, 1.0f, 1.0f) * 16.0f;
    lum = ShadeScene(worldPos, rayWS, l, rb, rt);

    lum = ACES(lum);

    outFragColor = vec4(lum, 1.0f);
}
