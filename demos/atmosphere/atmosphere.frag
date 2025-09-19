#version 450

layout(location = 0) out vec4 outFragColor;

#define PI 3.14159265f

const vec3 sRayleighScattering = vec3(5.802f, 13.558f, 33.1f);
const float sMieScattering = 3.996f;

const float sMieAbsorption = 4.40f;
const vec3 sOzoneAbsorption = vec3(0.65f, 1.881f, 0.085f);

float PhaseRayleigh(float theta)
{
    float c = cos(theta);
    return 3 * (1 + c * c) / (16 * PI);
}

float PhaseMie(float theta, float g)
{
    float g2 = g * g;
    float c = cos(theta);

    return (3.0f * (1.0f - g2) * (1.0f + c * c)) /
           pow(8.0f * PI * (2.0f + g2) * (1.0f + g2 - 2.0f * g * c), 1.5f);
}

float DensityRayleigh(float h) { return exp(-h / 8000.0f); }

float DensityMie(float h) { return exp(-h / 1200.0f); }

float DensityOzone(float h)
{
    return max(0.0f, 1.0f - abs(h - 25000.0f) / 15000.0f);
}

float ExtinctionRayleigh(float h)
{
    return sRayleighScattering * DensityRayleigh(h);
}

float ExtinctionMie(float h)
{
    return (sMieAbsorption + sMieScattering) * DensityMie(h);
}

void main() { outFragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f); }
