#include "bindless.glsl"

#define GOLDEN_RATIO 1.6180339f
#define PI 3.14159265359f
#define MIE_G 0.8f
#define PHASE_UNIFORM (1.0f / (4.0f * PI))
#define MIE_PHASE_SCALE (3.0f / (8.0f * PI))
#define RAYLEIGH_PHASE_SCALE (3.0f / (16.0f * PI))

FLY_REGISTER_UNIFORM_BUFFER(AtmosphereParams, {
    vec3 rayleighScattering;
    float mieScattering;
    vec3 ozoneAbsorption;
    float mieAbsorption;
    vec2 transmittanceMapDims;
    float rb;
    float rt;
    vec2 multiscatteringMapDims;
    vec2 skyviewMapDims;
    float zenith;
    float azimuth;
    float rayleighDensityCoeff;
    float mieDensityCoeff;
})

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)
FLY_REGISTER_STORAGE_TEXTURE_BUFFER(writeonly, StorageTexturesRGBA16F, image2D,
                                    rgba16f)

float SafeSqrt(float x) { return sqrt(max(0.0f, x)); }

// Note: Assuming ray direction d is normalized
float RaySphereIntersect(vec3 o, vec3 d, vec3 p, float r)
{
    float b = dot(o - p, d);
    float c = dot(o - p, o - p) - r * r;

    float disc = b * b - c;
    if (disc < 0.0f)
    {
        return -1.0f;
    }

    float t = (-b - SafeSqrt(disc));
    if (t < 0.0f)
    {
        t = (-b + SafeSqrt(disc));
    }
    return t;
}

float PhaseMie(float cosTheta, float g)
{
    float num = (1.0f - g * g) * (1.0f + cosTheta * cosTheta);
    float denom =
        (2.0f + g * g) * pow((1.0f + g * g - 2.0f * g * cosTheta), 1.5f);

    return MIE_PHASE_SCALE * num / denom;
}

float PhaseRayleigh(float cosTheta)
{
    return RAYLEIGH_PHASE_SCALE * (1.0f + cosTheta * cosTheta);
}

float DensityRayleigh(float h, uint atmosphereBufferIndex)
{
    float rayleighDensityCoeff = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, rayleighDensityCoeff);
    return exp(-h / rayleighDensityCoeff);
}

float DensityMie(float h, uint atmosphereBufferIndex)
{
    float mieDensityCoeff = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, mieDensityCoeff);
    return exp(-h / mieDensityCoeff);
}

float DensityOzone(float h) { return max(0.0f, 1.0f - abs(h - 25.0f) / 15.0f); }

vec3 SampleExtinction(float height, uint atmosphereBufferIndex)
{
    vec3 rayleighScattering = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, rayleighScattering);
    float mieScattering = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, mieScattering);
    vec3 ozoneAbsorption = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, ozoneAbsorption);
    float mieAbsorption = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, mieAbsorption);

    vec3 extinction = vec3(0.0f);
    extinction +=
        rayleighScattering * DensityRayleigh(height, atmosphereBufferIndex);
    extinction += (mieScattering + mieAbsorption) *
                  DensityMie(height, atmosphereBufferIndex);
    extinction += ozoneAbsorption * DensityOzone(height);

    return extinction;
}

vec3 SampleRayleighScattering(float height, uint atmosphereBufferIndex)
{
    vec3 rayleighScattering = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, rayleighScattering);
    return rayleighScattering * DensityRayleigh(height, atmosphereBufferIndex);
}

vec3 SampleMieScattering(float height, uint atmosphereBufferIndex)
{
    float mieScattering = FLY_ACCESS_UNIFORM_BUFFER(
        AtmosphereParams, atmosphereBufferIndex, mieScattering);

    return vec3(mieScattering * DensityMie(height, atmosphereBufferIndex));
}

vec3 SampleScattering(float height, uint atmosphereBufferIndex)
{
    return SampleRayleighScattering(height, atmosphereBufferIndex) +
           SampleMieScattering(height, atmosphereBufferIndex);
}

vec3 SphereCoordToRay(float phi, float theta)
{
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    return vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
}

vec2 TransmittanceRadiusCosZenithToUV(float r, float cosZ, float rb, float rt)
{
    float h = SafeSqrt(rt * rt - rb * rb);
    float rho = SafeSqrt(r * r - rb * rb);

    float dMin = rt - r;
    float dMax = rho + h;

    float disc = r * r * (cosZ * cosZ - 1) + rt * rt;
    // Note that d1 has no chance to be positive, so solution is only d2
    // d1 = -r * cosZ - SafeSqrt(det);
    // d2 = -r * cosZ + SafeSqrt(det);
    float d = max(0.0f, -r * cosZ + SafeSqrt(disc));

    float u = clamp(rho / h, 0.0f, 1.0f);
    float v = clamp((d - dMin) / (dMax - dMin), 0.0f, 1.0f);

    return vec2(u, v);
}

vec3 SampleTransmittance(float r, float cosZ, float rb, float rt,
                         uint transmittanceMapIndex)
{
    return texture(FLY_ACCESS_TEXTURE_BUFFER(Textures, transmittanceMapIndex),
                   TransmittanceRadiusCosZenithToUV(r, cosZ, rb, rt))
        .rgb;
}

vec2 MultiscatteringHeightCosZenithToUV(float height, float cosZ, float rb,
                                        float rt)
{
    float u = (cosZ + 1.0f) * 0.5f;
    float v = height / (rt - rb);
    return vec2(clamp(u, 0.0f, 1.0f), clamp(v, 0.0f, 1.0f));
}

vec3 SampleMultiscattering(float height, float cosZ, float rb, float rt,
                           uint multiscatteringMapIndex)
{
    return texture(FLY_ACCESS_TEXTURE_BUFFER(Textures, multiscatteringMapIndex),
                   MultiscatteringHeightCosZenithToUV(height, cosZ, rb, rt))
        .rgb;
}
