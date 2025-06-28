#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

#define CASCADE_COUNT 4
#define RATIO 4.31662479036f // 1 + sqrt(11)

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint heightMapCascades[4];
    uint diffDisplacementCascades[4];
    uint foamTextureIndex;
    float waveChopiness;
    float foamDecay;
    float foamGain;
    float deltaTime;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

float SampleDetJacobian(float bias)
{
    vec2 dxUV = dFdx(inUV);
    vec2 dyUV = dFdy(inUV);
    float scale = 1.0f;

    float dxDx = 0.0f;
    float dyDy = 0.0f;
    float dyDx = 0.0f;

    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        vec2 dxScaled = dxUV * scale * bias;
        vec2 dyScaled = dyUV * scale * bias;

        vec4 value = textureGrad(
            FLY_ACCESS_TEXTURE_BUFFER(
                Textures, gPushConstants.diffDisplacementCascades[i]),
            inUV * scale, dxScaled, dyScaled);

        vec4 h = textureGrad(FLY_ACCESS_TEXTURE_BUFFER(
                                 Textures, gPushConstants.heightMapCascades[i]),
                             inUV * scale, dxScaled, dyScaled);

        dxDx += value.z;
        dyDy += value.w;
        dyDx += h.w;
        scale *= RATIO;
    }

    vec3 jacobian =
        1.0f + gPushConstants.waveChopiness * vec3(dxDx, dyDy, dyDx);

    return jacobian.x * jacobian.y - jacobian.z * jacobian.z;
}

void main()
{
    float detJ = SampleDetJacobian(0.15f);
    float lastFoamValue =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Textures,
                                          gPushConstants.foamTextureIndex),
                inUV)
            .r;

    float energy = max(-detJ, 0.0f);
    float foam = lastFoamValue -
                 gPushConstants.foamDecay * gPushConstants.deltaTime +
                 energy * 0.01 * gPushConstants.foamGain;
    outFragColor = vec4(foam, 0.0f, 0.0f, 1.0f);
}
