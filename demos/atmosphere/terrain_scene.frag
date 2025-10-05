#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

#define MAX_MARCHING_STEPS 300
#define MAX_DIST 600.0f
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

// https://iquilezles.org/articles/filterableprocedurals/
float FilteredXor(vec2 p, vec2 dpdx, vec2 dpdy)
{
    float xor = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        vec2 w = max(abs(dpdx), abs(dpdy)) + 0.01f;
        vec2 f = 2.0f *
                 (abs(fract((p - 0.5f * w) / 2.0f) - 0.5f) -
                  abs(fract((p + 0.5f * w) / 2.0f) - 0.5f)) /
                 w;
        xor+= 0.5f - 0.5f * f.x* f.y;

        dpdx *= 0.5f;
        dpdy *= 0.5f;
        p *= 0.5f;
        xor*= 0.5f;
    }
    return xor;
}

float PlaneSdf(vec3 p, vec3 n, float h) { return dot(p, n) - 1; }

float SceneSdf(vec3 p) { return PlaneSdf(p, vec3(0, 1, 0), 0.0f); }

float RayMarch(vec3 origin, vec3 dir, float start, float end)
{
    float depth = start;
    for (int i = 0; i < MAX_MARCHING_STEPS; i++)
    {
        float dist = SceneSdf(origin + depth * dir);
        if (dist < EPSILON)
        {
            return depth;
        }
        depth += dist;
        if (depth >= end)
        {
            return -1.0f;
        }
    }
    return -1.0f;
}

vec3 IntegrateLuminance(vec3 origin, vec3 dir)
{
    vec3 lum = vec3(0.0f, 0.0f, 0.0f);
    float t = RayMarch(origin, dir, 0.0f, MAX_DIST);

    if (t > 0)
    {
        vec3 pos = origin + t * dir;
        vec2 p = pos.xz;
        vec2 dpdx = dFdx(p);
        vec2 dpdy = dFdy(p);

        lum = vec3(FilteredXor(p, dpdx, dpdy));
    }

    return lum;
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
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, zenith);
    float azimuth = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, azimuth);

    float cosZ = cos(radians(zenith));
    float sinZ = sin(radians(zenith));
    float cosA = cos(radians(azimuth));
    float sinA = sin(radians(azimuth));
    vec3 l = vec3(sinZ * sinA, cosZ, sinZ * cosA);

    vec3 e = vec3(1.0f, 1.0f, 1.0f) * 16.0f;

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

    vec3 lum = IntegrateLuminance(camPos, rayWS);

    lum = Reinhard(lum, 1.0f);

    outFragColor = vec4(lum, 1.0f);
}
