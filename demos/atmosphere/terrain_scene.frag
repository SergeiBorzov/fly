#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

#define MAX_MARCHING_STEPS 600
#define MAX_MARCHING_DIST 50000.0f
#define EPSILON 0.002f

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

vec2 RandomGradient(vec2 x)
{
    x = fract(x * 0.3183099f + 0.1f) * 17.0f;
    float a = fract(x.x * x.y * (x.x + x.y));
    a *= 2.0 * PI;
    return vec2(sin(a), cos(a));
}

float GradientNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    // quintic smoothstep
    vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    return mix(
        mix(dot(RandomGradient(i + vec2(0, 0)), f - vec2(0.0, 0.0)),
            dot(RandomGradient(i + vec2(1, 0)), f - vec2(1.0, 0.0)), u.x),
        mix(dot(RandomGradient(i + vec2(0, 1)), f - vec2(0.0, 1.0)),
            dot(RandomGradient(i + vec2(1, 1)), f - vec2(1.0, 1.0)), u.x),
        u.y);
}

float TerrainSdf(vec3 p) { return p.y - 100.0f * GradientNoise(p.xz / 1000).x; }

float SceneSdf(vec3 p) { return TerrainSdf(p); }

vec3 Normal(vec3 p)
{
    return normalize(vec3(SceneSdf(p + vec3(EPSILON, 0.0f, 0.0f)) -
                              SceneSdf(p - vec3(EPSILON, 0.0f, 0.0f)),
                          SceneSdf(p + vec3(0.0f, EPSILON, 0.0f)) -
                              SceneSdf(p - vec3(0.0f, EPSILON, 0.0f)),
                          SceneSdf(p + vec3(0.0f, 0.0f, EPSILON)) -
                              SceneSdf(p - vec3(0.0f, 0.0f, EPSILON))));
}

int RayMarch(vec3 origin, vec3 dir, float start, float end, out float depth)
{
    depth = start;
    int res = -1;
    for (int i = 0; i < MAX_MARCHING_STEPS; i++)
    {
        float dist = SceneSdf(origin + depth * dir);
        depth += dist;
        if (dist < EPSILON * depth)
        {
            return 0;
        }
        if (depth >= end)
        {
            return -1;
        }
    }
    return -1;
}

// Appendix B:
// https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
vec3 LimbDarkening(vec3 luminance, float centerToEdge)
{
    const vec3 a0 = vec3(0.34685f, 0.26073f, 0.15248f);
    const vec3 a1 = vec3(1.37539f, 1.27428f, 1.38517f);
    const vec3 a2 = vec3(-2.04425f, -1.30352f, -1.49615f);
    const vec3 a3 = vec3(2.70493f, 1.47085f, 1.99886f);
    const vec3 a4 = vec3(-1.94290f, -0.96618f, -1.48155f);
    const vec3 a5 = vec3(0.55999f, 0.26384f, 0.44119f);

    vec3 mu = vec3(sqrt(1.0f - centerToEdge * centerToEdge));
    vec3 factor = a0 + mu * (a1 + mu * (a2 + mu * (a3 + mu * (a4 + mu * a5))));

    return luminance * factor;
}

vec3 ShadeScene(vec3 origin, vec3 dir, vec3 e, vec3 l, float rb, float rt)
{
    vec3 lum = vec3(0.0f);

    float t = 0.0f;
    int id = RayMarch(origin, dir, 0.0, MAX_MARCHING_DIST, t);

    if (id >= 0)
    {
        vec3 hitPoint = origin + dir * t;
        vec3 n = Normal(hitPoint);
        lum = vec3(10000.0f, 0.0f, 0.0f) * max(dot(n, l), 0.0f);
    }
    else
    {
        if (dir.y > 0.0f)
        {
            vec3 worldPos = origin * 0.001f;
            worldPos.y += rb;

            // Sky
            float lat = asin(dir.y);
            float lon = atan(dir.x, dir.z);
            lum = SampleSkyview(lon, lat, gPushConstants.skyviewMapIndex) * 1e5;

            // Sun
            float sunAngularDiameterRadians = FLY_ACCESS_UNIFORM_BUFFER(
                AtmosphereParams, gPushConstants.atmosphereBufferIndex,
                sunAngularDiameterRadians);
            float sunAngularRadius = sunAngularDiameterRadians * 0.5f;
            float r = length(worldPos);
            float cosZ = dot(dir, normalize(worldPos));
            float dotDirL = dot(dir, l);
            float k = smoothstep(cos(sunAngularRadius * 1.01f),
                                 cos(sunAngularRadius), dotDirL);
            float vis =
                float(RaySphereIntersect(worldPos, dir, vec3(0.0f), rb) < 0.0f);

            vec3 sunLum =
                k * e *
                SampleTransmittance(r, cosZ, rb, rt,
                                    gPushConstants.transmittanceMapIndex);
            float centerToEdge =
                clamp(acos(dotDirL) / sunAngularRadius, 0.0f, 1.0f);
            sunLum = LimbDarkening(sunLum, centerToEdge);

            lum += vis * sunLum *
                   5e-3; // physically accurate value still too bright
        }
    }

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

float EvFromCosZenith(float cosZenith)
{
    // Clamp range to roughly -0.309..1.0 (corresponding to -18°..0° below
    // horizon) cos(108°) ≈ -0.309 — below that we stay at night EV
    float t = clamp((cosZenith + 0.309) / (1.0 + 0.309), 0.0, 1.0);

    // Apply a perceptual gamma to slow transition near horizon/twilight
    t = pow(t, 0.05);

    // Interpolate between night EV (-2) and bright daylight (16)
    return mix(-2.0, 12.0, t);
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

    vec3 lum = vec3(0.0f);

    vec3 sunAlbedo = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, sunAlbedo);
    vec3 sunLuminance = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunLuminanceOuterSpace);
    vec3 e = sunAlbedo * sunLuminance;

    lum = ShadeScene(camPos, rayWS, e, l, rb, rt);
    vec3 worldPos = camPos * 0.001f;
    camPos.y += rb;
    lum *= exp2(-EvFromCosZenith(dot(l, normalize(worldPos))));
    lum = ACES(lum);

    outFragColor = vec4(lum, 1.0f);
}
