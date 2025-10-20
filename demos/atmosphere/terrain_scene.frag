#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

#define MAX_MARCHING_STEPS 600
#define MAX_MARCHING_DIST 50000.0f
#define EPSILON 0.002f
#define SCALE 100000000
#define LUMINANCE_SCALE 100

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint atmosphereBufferIndex;
    uint cameraBufferIndex;
    uint skyRadianceProjectionBufferIndex;
    uint averageHorizonLuminanceBufferIndex;
    uint transmittanceMapIndex;
    uint skyviewMapIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, RadianceProjectionSH, {
    ivec3 coefficient;
    float pad;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, AverageLuminance, {
    ivec3 luminance;
    float pad;
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
    for (int i = 0; i < 8; i++)
    {
        vec3 n = GradientNoise(f * p);
        d.x += a * n.x;
        d.yz += a * f * n.yz;
        f *= 2.0;
        a *= g;
    }
    // Doing more octaves for normals to get details
    // Yet less octaves for point itself to avoid shadow artifacts with
    // precision
    for (int i = 0; i < 4; i++)
    {
        vec3 n = GradientNoise(f * p);
        d.yz += a * f * n.yz;
        f *= 2.0f;
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
            d.x = depth;
            return 0;
        }
        if (depth >= end)
        {
            return -1;
        }
    }
    return -1;
}

vec3 IrradianceSH9(vec3 n, vec3 l[9])
{
    const float c1 = 0.429043f;
    const float c2 = 0.511664f;
    const float c3 = 0.743125f;
    const float c4 = 0.886227f;
    const float c5 = 0.247708f;

    vec3 m00 = c1 * l[8];
    vec3 m01 = c1 * l[4];
    vec3 m02 = c1 * l[7];
    vec3 m03 = c2 * l[3];
    vec3 m11 = -c1 * l[8];
    vec3 m12 = c1 * l[5];
    vec3 m13 = c2 * l[1];
    vec3 m22 = c3 * l[6];
    vec3 m23 = c2 * l[2];
    vec3 m33 = c4 * l[0] - c5 * l[6];

    mat4 Mr = mat4(
        vec4(m00.r, m01.r, m02.r, m03.r), vec4(m01.r, m11.r, m12.r, m13.r),
        vec4(m02.r, m12.r, m22.r, m23.r), vec4(m03.r, m13.r, m23.r, m33.r));
    mat4 Mg = mat4(
        vec4(m00.g, m01.g, m02.g, m03.g), vec4(m01.g, m11.g, m12.g, m13.g),
        vec4(m02.g, m12.g, m22.g, m23.g), vec4(m03.g, m13.g, m23.g, m33.g));
    mat4 Mb = mat4(
        vec4(m00.b, m01.b, m02.b, m03.b), vec4(m01.b, m11.b, m12.b, m13.b),
        vec4(m02.b, m12.b, m22.b, m23.b), vec4(m03.b, m13.b, m23.b, m33.b));

    vec4 n4 = vec4(vec3(-n.x, n.z, -n.y), 1.0f);
    return vec3(dot(n4, Mr * n4), dot(n4, Mg * n4), dot(n4, Mb * n4));
}

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

float ExponentialHeightFog(vec3 worldPos, vec3 camPos, vec3 camDir, float L)
{
    const float fogAbsorption = 0.01f;
    float fogDensity = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        exponentialFogDensity);
    float fogFalloff = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        exponentialFogFalloff);

    float opticalDepth =
        fogDensity * exp(-camPos.y / fogFalloff) *
        (1.0f - exp(-camDir.y * L / fogFalloff)) /
        (sign(camDir.y) * max(abs(camDir.y), 0.00001f) / fogFalloff);

    return exp(-fogAbsorption * opticalDepth);
}

vec3 ShadeScene(vec3 origin, vec3 dir, vec3 l, float rb, float rt)
{
    vec3 skyRadianceProjection[9];
    for (uint i = 0; i < 9; i++)
    {
        skyRadianceProjection[i] =
            vec3(FLY_ACCESS_STORAGE_BUFFER(
                     RadianceProjectionSH,
                     gPushConstants.skyRadianceProjectionBufferIndex)[i]
                     .coefficient) /
            SCALE;
    }
    vec3 averageHorizonLuminance =
        vec3(FLY_ACCESS_STORAGE_BUFFER(
                 AverageLuminance,
                 gPushConstants.averageHorizonLuminanceBufferIndex)[0]
                 .luminance) /
        LUMINANCE_SCALE;

    vec3 sunAlbedo = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex, sunAlbedo);
    float sunAngularDiameterRadians = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunAngularDiameterRadians);
    float sunAngularRadius = sunAngularDiameterRadians * 0.5f;
    vec3 luminance = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunLuminanceOuterSpace);
    vec3 illuminance = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, gPushConstants.atmosphereBufferIndex,
        sunIlluminanceOuterSpace);

    vec3 sunLuminanceOuterSpace = sunAlbedo * luminance;
    vec3 sunIlluminanceOuterSpace = sunAlbedo * illuminance;

    vec3 lum = vec3(0.0f);

    vec3 d = vec3(0.0f);
    int id = RayMarch(origin, dir, 0.0, MAX_MARCHING_DIST, d);

    float r = origin.y * 0.001f + rb;
    vec3 worldPos = vec3(0.0f, r, 0.0f);
    float cosZ = l.y;

    float lat = asin(dir.y);
    float lon = atan(dir.x, dir.z);
    vec3 horizonColor =
        SampleSkyview(lon, 0.0f, gPushConstants.skyviewMapIndex);

    float cosTheta = dot(dir, l);
    float phase = mix(PhaseMie(cosTheta, MIE_G), PhaseRayleigh(cosTheta), 0.5);
    phase = mix(phase, 1.0f, 0.75f);
    vec3 fogColor = averageHorizonLuminance * phase;

    if (id >= 0)
    {
        vec3 sunLuminance =
            sunLuminanceOuterSpace *
            SampleTransmittance(r, cosZ, rb, rt,
                                gPushConstants.transmittanceMapIndex);

        vec3 hitPoint = origin + dir * d.x;
        vec3 n = normalize(vec3(-d.y, 1.0f, -d.z));

        vec3 shadowD;
        int id =
            RayMarch(hitPoint + n * 0.02f, l, 0.0f, MAX_MARCHING_DIST, shadowD);
        float vis = float(id == -1);

        vec3 f = vec3(0.2f, 0.2f, 0.2f) / PI; // Terrain BRDF
        // sun contribution
        lum += vis * f * sunLuminance * max(dot(n, l), 0.0f) * PI * 0.5f *
               (1 - cos(sunAngularDiameterRadians));

        // sky contribution
        vec3 skyIrradiance =
            IrradianceSH9(vec3(-n.z, -n.x, n.y), skyRadianceProjection);
        lum += f * skyIrradiance;

        float fogFactor = ExponentialHeightFog(hitPoint, origin, dir, d.x);
        lum = mix(fogColor, lum, fogFactor);
    }
    else
    {
        float dist =
            RaySphereIntersect(worldPos, dir, vec3(0.0f), rt) * 1000.0f;
        vec3 hitPoint = origin + dir * dist;
        float fogFactor = ExponentialHeightFog(hitPoint, origin, dir, dist);

        if (dir.y > 0.0f)
        {
            // Sky
            lum = SampleSkyview(lon, lat, gPushConstants.skyviewMapIndex);

            // Sun
            float cosZFromView = dir.y;
            float k = smoothstep(cos(sunAngularRadius * 1.01f),
                                 cos(sunAngularRadius), cosTheta);
            float vis =
                float(RaySphereIntersect(worldPos, dir, vec3(0.0f), rb) < 0.0f);

            vec3 sunLum =
                k * sunLuminanceOuterSpace *
                SampleTransmittance(r, cosZFromView, rb, rt,
                                    gPushConstants.transmittanceMapIndex);
            float centerToEdge =
                clamp(acos(cosTheta) / sunAngularRadius, 0.0f, 1.0f);
            sunLum = LimbDarkening(sunLum, centerToEdge);

            lum += vis * sunLum;
        }
        else
        {
            lum = horizonColor;
        }
        lum = mix(fogColor, lum, fogFactor);
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

    vec3 lum = ShadeScene(camPos, rayWS, l, rb, rt);
    lum *= exp2(-EvFromCosZenith(l.y));
    // lum *= exp2(-12.0f);
    lum = ACES(lum);

    outFragColor = vec4(lum, 1.0f);
}
