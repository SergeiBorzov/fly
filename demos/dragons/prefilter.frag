#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_multiview : require
#include "bindless.glsl"

#define PI 3.14159265359f
#define SAMPLE_COUNT 1024

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint environmentMapIndex;
    uint prefilteredMipLevel;
    uint prefilteredMipCount;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

vec3 GetViewDirection(vec2 coord, uint faceIndex)
{
    if (faceIndex == 0)
    {
        return normalize(vec3(1.0f, -coord.y, -coord.x));
    }
    else if (faceIndex == 1)
    {
        return normalize(vec3(-1.0f, -coord.y, coord.x));
    }
    else if (faceIndex == 2)
    {
        return normalize(vec3(coord.x, 1.0f, coord.y));
    }
    else if (faceIndex == 3)
    {
        return normalize(vec3(coord.x, -1.0f, -coord.y));
    }
    else if (faceIndex == 4)
    {
        return normalize(vec3(coord.x, -coord.y, 1.0f));
    }
    else
    {
        return normalize(vec3(-coord.x, -coord.y, -1.0f));
    }
}

float RadicalInverse2(uint n)
{
    // Note that digits of radical inverse with base 2 (Van der Corput sequence)
    // Is just the reverse of the bits
    n = ((n >> 1) & 0x55555555u) | ((n & 0x55555555u) << 1);
    n = ((n >> 2) & 0x33333333u) | ((n & 0x33333333u) << 2);
    n = ((n >> 4) & 0x0F0F0F0Fu) | ((n & 0x0F0F0F0Fu) << 4);
    n = ((n >> 8) & 0x00FF00FFu) | ((n & 0x00FF00FFu) << 8);
    n = (n >> 16) | (n << 16);

    return float(n) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse2(i));
}

vec3 ImportanceSampleGGX(vec2 p, vec3 n, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * PI * p.x;
    float cosTheta = sqrt((1.0f - p.y) / (1.0f + (a * a - 1.0f) * p.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

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
    vec3 n = GetViewDirection(inUV * 2.0f - 1.0f, gl_ViewIndex);
    vec3 v = n; // Another crude approximation

    float roughness = float(gPushConstants.prefilteredMipLevel) /
                      gPushConstants.prefilteredMipCount;

    // Cosine weighting is from:
    // https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
    float totalWeight = 0.0f;
    vec3 color = vec3(0.0f);
    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {
        vec2 p = Hammersley(i, SAMPLE_COUNT);
        vec3 h = ImportanceSampleGGX(p, n, roughness);
        vec3 l = normalize(reflect(-v, h));

        float nl = max(dot(n, l), 0.0f);
        if (nl > 0.0f)
        {
            color += texture(FLY_ACCESS_TEXTURE_BUFFER(
                                 Cubemaps, gPushConstants.environmentMapIndex),
                             l)
                         .rgb;
            totalWeight += nl;
        }
    }
    color /= totalWeight;

    outFragColor = vec4(color, 1.0f);
}
