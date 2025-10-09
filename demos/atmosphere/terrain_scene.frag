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

vec3 GradientNoise(vec2 p)
{
    ivec2 i = ivec2(floor(p));
    vec2 f = fract(p);

    // quintic smoothstep
    vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    vec2 du = 30.0f * f * f * (f * (f - 2.0f) + 1.0f);

    vec2 g00 = RandomGradient(i + vec2(0, 0));
    vec2 g01 = RandomGradient(i + vec2(1, 0));
    vec2 g10 = RandomGradient(i + vec2(0, 1));
    vec2 g11 = RandomGradient(i + vec2(1, 1));

    float n00 = dot(g00, f - vec2(0, 0));
    float n01 = dot(g01, f - vec2(1, 0));
    float n10 = dot(g10, f - vec2(0, 1));
    float n11 = dot(g11, f - vec2(1, 1));

    float value = mix(mix(n00, n01, u.x), mix(n10, n11, u.x), u.y);

    float dx = mix(mix(g00.x, g01.x, u.x), mix(g10.x, g11.x, u.x), u.y) +
               du.x * ((n01 - n00) + u.y * (n11 - n10 - n01 + n00));
    float dy = mix(mix(g00.y, g01.y, u.x), mix(g10.y, g11.y, u.x), u.y) +
               du.y * ((n10 - n00) + u.x * (n11 - n10 - n01 + n00));

    return vec3(value, dx, dy);
}

vec3 FBM(vec2 p)
{
    vec3 d = vec3(0.0f);
    float g = 0.5f;
    float f = 0.00005f;
    float a = 1.0;

    float t = 0.0;
    for (int i = 0; i < 12; i++)
    {
        vec3 n = GradientNoise(f * p);
        d.x += a * n.x;
        d.yz += a * f * n.yz;
        // d.x += a * n.x;
        // d.yz += a * n.yz;
        f *= 2.0;
        a *= g;
    }
    return d;
}

vec3 TerrainSdf(vec3 p)
{
    vec3 value = 2500.0f * FBM(p.xz);
    return vec3(p.y - value.x, value.yz);
}

vec3 SceneSdf(vec3 p) { return TerrainSdf(p); }

// vec3 Normal(vec3 p)
// {
//     return normalize(vec3(SceneSdf(p + vec3(EPSILON, 0.0f, 0.0f)) -
//                               SceneSdf(p - vec3(EPSILON, 0.0f, 0.0f)),
//                           SceneSdf(p + vec3(0.0f, EPSILON, 0.0f)) -
//                               SceneSdf(p - vec3(0.0f, EPSILON, 0.0f)),
//                           SceneSdf(p + vec3(0.0f, 0.0f, EPSILON)) -
//                               SceneSdf(p - vec3(0.0f, 0.0f, EPSILON))));
// }

int RayMarch(vec3 origin, vec3 dir, float start, float end, out vec3 d)
{
    float depth = start;
    int res = -1;
    for (int i = 0; i < MAX_MARCHING_STEPS; i++)
    {
        d = SceneSdf(origin + depth * dir);
        depth += d.x;
        if (d.x < EPSILON * depth)
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

    vec3 d = vec3(0.0f);
    int id = RayMarch(origin, dir, 0.0, MAX_MARCHING_DIST, d);

    float r = origin.y * 0.001f + rb;
    vec3 worldPos = vec3(0.0f, r, 0.0f);
    float cosZ = l.y;

    if (id >= 0)
    {
        vec3 sunLum =
            e * SampleTransmittance(r, cosZ, rb, rt,
                                    gPushConstants.transmittanceMapIndex);

        vec3 hitPoint = origin + dir * d.x;
        vec3 n = normalize(vec3(-d.y, 1.0f, -d.z));
        // vec3 n = Normal(hitPoint);
        lum = vec3(0.1f, 0.1f, 0.1f) * sunLum * max(dot(n, l), 0.0f) * 5e-4;
    }
    else
    {
        if (dir.y > 0.0f)
        {
            // vec3 worldPos = origin * 0.001f;
            // worldPos.y += rb;

            // Sky
            float lat = asin(dir.y);
            float lon = atan(dir.x, dir.z);
            lum = SampleSkyview(lon, lat, gPushConstants.skyviewMapIndex) * 1e5;

            // Sun
            float sunAngularDiameterRadians = FLY_ACCESS_UNIFORM_BUFFER(
                AtmosphereParams, gPushConstants.atmosphereBufferIndex,
                sunAngularDiameterRadians);
            float sunAngularRadius = sunAngularDiameterRadians * 0.5f;

            float cosZFromView = dir.y;
            float dotDirL = dot(dir, l);
            float k = smoothstep(cos(sunAngularRadius * 1.01f),
                                 cos(sunAngularRadius), dotDirL);
            float vis =
                float(RaySphereIntersect(worldPos, dir, vec3(0.0f), rb) < 0.0f);

            vec3 sunLum =
                k * e *
                SampleTransmittance(r, cosZFromView, rb, rt,
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
    return mix(1.0, 13.0, t);
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
    lum *= exp2(-EvFromCosZenith(l.y));
    // lum *= exp2(-12.0f);
    lum = ACES(lum);

    outFragColor = vec4(lum, 1.0f);
}
