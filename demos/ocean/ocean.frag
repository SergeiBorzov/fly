#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

#define CASCADE_COUNT 4

layout(location = 0) in VertexData
{
    vec3 view;
    vec2 uv;
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

void main()
{
    vec2 slope = SampleSlope(0.5f);

    vec3 n = normalize(vec3(-slope.x, 1.0f, -slope.y));
    vec3 v = normalize(inData.view);
    float fresnel = pow(1.0 - max(0.15, dot(n, v)), 5.0);

    outColor = vec4(vec3(fresnel), 1.0f);
}
