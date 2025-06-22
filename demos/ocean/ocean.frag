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
    uint shadeParamsBufferIndex;
    uint vertexBufferIndex;
    uint heightMapCascades[4];
    uint diffDisplacementCascades[4];
    uint skyBoxTextureIndex;
    float waveChopiness;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(ShadeParams, {
    vec4 lightColorReflectivity;
    vec4 waterScatterColor;
    vec4 coefficients; // s1, s2, a1, a2
    vec4 bubbleColorDensity;
})

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
    vec2 dxdyDisplacement = vec2(0.0f);

    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        vec2 dxScaled = dxUV * scale * bias;
        vec2 dyScaled = dyUV * scale * bias;

        vec4 value = textureGrad(
            FLY_ACCESS_TEXTURE_BUFFER(
                Textures, gPushConstants.diffDisplacementCascades[i]),
            inData.uv * scale, dxScaled, dyScaled);

        slope += value.xy;
        dxdyDisplacement += value.zw;
        scale *= 4.0f;
    }

    // Correct normal after displacement
    slope /= (1.0 + gPushConstants.waveChopiness * dxdyDisplacement);
    return slope;
}

float Fresnel(vec3 n, vec3 v) { return pow(1.0 - max(0.0, dot(n, v)), 5.0); }

float SpecularBlinnPhong(vec3 n, vec3 h, float alpha)
{
    return pow(max(dot(n, h), 0.0f), alpha);
}

vec3 ReflectedColor(vec3 fresnel, vec3 r, float k)
{
    return k * fresnel *
           texture(FLY_ACCESS_TEXTURE_BUFFER(Cubemaps,
                                             gPushConstants.skyBoxTextureIndex),
                   r)
               .rgb;
}

// Fake subsurface scattering by Atlas guys:
// https://www.youtube.com/watch?v=Dqld965-Vv0&ab_channel=GameDevelopersConference
vec3 SubsurfaceScattering(vec3 fresnel, vec3 l, vec3 n, vec3 v, float k1,
                          float k2)
{
    float ss1 = k1 * max(0, inData.height) * pow(max(dot(l, -v), 0.0f), 4.0f) *
                pow(0.5f - 0.5f * (max(dot(l, n), 0.0f)), 3.0f);
    float ss2 = k2 * (pow(max(dot(v, n), 0.0f), 2.0f));

    return (vec3(1.0f) - fresnel) * vec3(ss1 + ss2);
}

// Ambient is from the same guys:
// https://www.youtube.com/watch?v=Dqld965-Vv0&ab_channel=GameDevelopersConference
// Basically diffuse + some bubble color
vec3 AmbientColor(vec3 l, vec3 n, float k1, float k2, float bubbleDensity,
                  vec3 waterScatterColor, vec3 lightColor, vec3 bubbleColor)
{
    return k1 * max(dot(l, n), 0.0f) * waterScatterColor * lightColor +
           k2 * bubbleDensity * bubbleColor;
}

vec3 FresnelSchlick(vec3 n, vec3 v, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - max(dot(n, v), 0.0f), 5.0);
}

void main()
{
    vec4 lightColorReflectivity = FLY_ACCESS_UNIFORM_BUFFER(
        ShadeParams, gPushConstants.shadeParamsBufferIndex,
        lightColorReflectivity);
    vec4 waterScatterColor = FLY_ACCESS_UNIFORM_BUFFER(
        ShadeParams, gPushConstants.shadeParamsBufferIndex, waterScatterColor);
    vec4 coefficients = FLY_ACCESS_UNIFORM_BUFFER(
        ShadeParams, gPushConstants.shadeParamsBufferIndex, coefficients);
    vec4 bubbleColorDensity = FLY_ACCESS_UNIFORM_BUFFER(
        ShadeParams, gPushConstants.shadeParamsBufferIndex, bubbleColorDensity);

    vec2 slope = SampleSlope(0.15f);
    vec3 n = normalize(vec3(-slope.x, 1.0f, -slope.y));

    vec3 l = normalize(vec3(1.0f, 1.0f, 1.0f));
    vec3 v = normalize(inData.view);
    vec3 h = normalize(l + v);
    vec3 r = reflect(-v, n);

    vec3 fresnel = FresnelSchlick(n, v, vec3(0.02f));

    vec3 specularColor =
        SpecularBlinnPhong(n, h, 250.0f) * lightColorReflectivity.xyz;
    vec3 reflectedColor = ReflectedColor(fresnel, r, lightColorReflectivity.w);

    vec3 ssColor =
        SubsurfaceScattering(fresnel, l, n, v, coefficients.x, coefficients.y) *
        waterScatterColor.xyz * lightColorReflectivity.xyz;

    vec3 ambientColor =
        AmbientColor(l, n, coefficients.z, coefficients.w, bubbleColorDensity.w,
                     waterScatterColor.xyz, lightColorReflectivity.xyz,
                     bubbleColorDensity.xyz);

    outColor =
        vec4(ambientColor + ssColor + specularColor + reflectedColor, 1.0f);
}
