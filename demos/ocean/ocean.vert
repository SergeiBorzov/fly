#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

#define CASCADE_COUNT 4
#define RATIO 4.31662479036f // 1 + sqrt(11)

layout(location = 0) out VertexData
{
    vec3 view;
    vec2 uv;
    float height;
}
outData;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint shadeParamsBufferIndex;
    uint vertexBufferIndex;
    uint heightMapCascades[4];
    uint diffDisplacementCascades[4];
    uint skyBoxTextureIndex;
    uint foamTextureIndex;
    float waveChopiness;
}
gPushConstants;

const vec2 positions[6] =
    vec2[](vec2(-0.5f, -0.5f), vec2(0.5f, -0.5f), vec2(-0.5f, 0.5f),
           vec2(-0.5f, 0.5f), vec2(0.5f, -0.5f), vec2(0.5f, 0.5f));

FLY_REGISTER_UNIFORM_BUFFER(UniformData, {
    mat4 projection;
    mat4 view;
    vec4 screenSize;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Vertex, {
    vec2 position;
    vec2 uv;
})

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

vec3 SampleHeightDisplacement(vec2 uv)
{
    float height = 0.0f;
    float scale = 1.0f;
    vec2 displacement = vec2(0.0f);

    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        vec3 h = texture(FLY_ACCESS_TEXTURE_BUFFER(
                             Textures, gPushConstants.heightMapCascades[i]),
                         uv * scale)
                     .rgb;
        displacement += h.yz;
        height += h.x;
        scale *= RATIO;
    }

    return vec3(height, displacement);
}

void main()
{
    Vertex vertex = FLY_ACCESS_STORAGE_BUFFER(
        Vertex, gPushConstants.vertexBufferIndex)[gl_VertexIndex];
    outData.uv = vertex.uv;

    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, view);

    vec3 h = SampleHeightDisplacement(vertex.uv);
    outData.height = h.x;
    h.yz *= gPushConstants.waveChopiness;

    vec3 worldPos = vec3(vertex.position.x + h.y, h.x, vertex.position.y + h.z);

    mat3 R = mat3(view);
    vec3 T = vec3(view[3]);
    vec3 camPos = -transpose(R) * T;
    outData.view = normalize(camPos - worldPos);

    gl_Position = projection * view * vec4(worldPos, 1.0f);
}
