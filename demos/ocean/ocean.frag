#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

#define CASCADE_COUNT 4

layout(location = 0) in VertexData
{
    vec3 view;
    vec2 uv;
    float height;
}
inData;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint vertexBufferIndex;
    uint heightMapCascades[4];
    uint diffDisplacementCascades[4];
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)
FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

// Since we scale uvs a lot for cascades, we create big derivatives
// Original texture function is to aggressive with filtering
// Bias allows to scaled down derivatives making filtering less aggressive
vec2 SampleSlope(float bias)
{
    vec2 dxUV = dFdx(inData.uv);
    vec2 dyUV = dFdy(inData.uv);
    float scale = 1.0f;
    vec2 slope = vec2(0.0f);

    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        vec2 dxScaled = dxUV * scale * bias;
        vec2 dyScaled = dyUV * scale * bias;

        vec4 value = textureGrad(
            FLY_ACCESS_TEXTURE_BUFFER(
                Textures, gPushConstants.diffDisplacementCascades[i]),
            inData.uv * scale, dxScaled, dyScaled);

        slope += value.xy;
        scale *= 4.0f;
    }
    return slope;
}

float Fresnel(vec3 n, vec3 v) { return pow(1.0 - max(0.0, dot(n, v)), 5.0); }

float SpecularBlinnPhong(vec3 n, vec3 h, float alpha)
{
    return pow(max(dot(n, h), 0.0f), alpha);
}

vec3 ReflectedColor(float fresnel, vec3 r, float k)
{
    return fresnel * k *
           texture(FLY_ACCESS_TEXTURE_BUFFER(Cubemaps,
                                             gPushConstants.skyBoxTextureIndex),
                   r)
               .rgb;
}

// Fake subsurface scattering by Atlas guys:
// https://www.youtube.com/watch?v=Dqld965-Vv0&ab_channel=GameDevelopersConference
vec3 SubsurfaceScattering(float fresnel, vec3 l, vec3 n, vec3 v, float k1,
                          float k2)
{
    float ss1 = k1 * max(0, inData.height) * pow(max(dot(l, -v), 0.0f), 4.0f) *
                pow(0.5f - 0.5f * (max(dot(l, n), 0.0f)), 3.0f);
    float ss2 = k2 * (pow(max(dot(v, n), 0.0f), 2.0f));

    return vec3((1 - fresnel) * (ss1 + ss2));
}

// Ambient is from the same guys:
// https://www.youtube.com/watch?v=Dqld965-Vv0&ab_channel=GameDevelopersConference
vec3 AmbientColor(vec3 l, vec3 n, float k1, float k2, float bubbleDensity,
                  vec3 waterSubsurfaceColor, vec3 lightColor, vec3 bubbleColor)
{
    return k1 * max(dot(l, n), 0.0f) * waterSubsurfaceColor * lightColor +
           k2 * bubbleDensity * bubbleColor;
}

void main()
{
    vec2 slope = SampleSlope(0.5f);

    vec3 l = normalize(vec3(1.0f, 1.0f, 0.0f));
    vec3 lightColor = vec3(1.0f, 0.843f, 0.702f);
    vec3 waterSubsurfaceColor = vec3(0.02, 0.1, 0.15) * 10;
    vec3 bubbleColor = vec3(1.0f);

    vec3 n = normalize(vec3(-slope.x, 1.0f, -slope.y));
    vec3 v = normalize(inData.view);
    vec3 h = normalize(l + v);
    vec3 r = reflect(-v, n);

    float fresnel = pow(1.0 - max(0.0f, dot(n, v)), 5.0);

    vec3 specularColor = SpecularBlinnPhong(n, h, 550.0f) * lightColor;
    vec3 reflectedColor = ReflectedColor(fresnel, r, 1.0f);

    float k1 = 24.0f;
    float k2 = 0.1f;
    float k3 = 0.01f;
    float k4 = 0.1f;
    vec3 ssColor = SubsurfaceScattering(fresnel, l, n, v, k1, k2) *
                   waterSubsurfaceColor * lightColor;

    vec3 ambientColor = AmbientColor(l, n, k3, k4, 0.05f, waterSubsurfaceColor,
                                     lightColor, bubbleColor);

    outColor =
        vec4(ambientColor + ssColor + specularColor + reflectedColor, 1.0f);
}
