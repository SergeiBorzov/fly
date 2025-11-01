#version 450

#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"

layout(push_constant) uniform Indices
{
    uint sceneDataIndex;
    uint textureIndex;
}
gIndices;

layout(location = 0) out vec2 outUVs;
layout(location = 1) out vec3 outColor;

FLY_REGISTER_UNIFORM_BUFFER(SceneData, {
    mat4 projection;
    mat4 view;
    float time;
})

const vec3 cubePositions[] = vec3[](
    vec3(0.5f, -0.5f, -0.5f), vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, -0.5f), vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),

    vec3(0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f), vec3(0.5f, -0.5f, -0.5f), vec3(0.5f, 0.5f, -0.5f),

    vec3(-0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f), vec3(0.5f, -0.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f),

    vec3(-0.5f, -0.5f, -0.5f), vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(-0.5f, 0.5f, -0.5f),
    vec3(-0.5f, -0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f),

    vec3(0.5f, 0.5f, -0.5f), vec3(-0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f), vec3(-0.5f, 0.5f, -0.5f), vec3(-0.5f, 0.5f, 0.5f),

    vec3(0.5f, -0.5f, 0.5f), vec3(-0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, -0.5f), vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, -0.5f, -0.5f));

const vec2 uvs[] = vec2[](vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(1.0f, 1.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f),

                          vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(1.0f, 1.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f),

                          vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(1.0f, 1.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f),

                          vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(1.0f, 1.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f),

                          vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(1.0f, 1.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f),

                          vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(1.0f, 1.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f));

const vec3 faceColors[] = vec3[](
    vec3(1.0f, 1.0f, 1.0f), vec3(1.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 1.0f));

mat3 RotateY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, 0.0f, s, 0.0f, 1.0f, 0.0f, -s, 0.0f, c);
}

mat3 RotateX(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(1.0f, 0.0f, 0.0f, 0.0f, c, -s, 0.0f, s, c);
}

mat3 RotateZ(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f);
}

float Rand(float co) { return fract(sin(co * (91.3458)) * 47453.5453); }

void main()
{
    outUVs = uvs[gl_VertexIndex];
    outColor = faceColors[gl_VertexIndex / 6];

    vec3 origin =
        vec3(gl_InstanceIndex % 100 - 5, gl_InstanceIndex / 100 - 5, 0);
    float r = Rand(gl_InstanceIndex);
    vec3 direction =
        vec3(r, Rand(gl_InstanceIndex % 7), Rand(gl_InstanceIndex % 21)) *
            10.0f -
        vec3(5.0f);

    float time =
        FLY_ACCESS_UNIFORM_BUFFER(SceneData, gIndices.sceneDataIndex, time);
    vec3 translate = direction * time;
    float angle = time * r * 5.0f;
    vec3 worldPos = (RotateY(angle) * RotateX(angle) * RotateZ(angle) *
                     cubePositions[gl_VertexIndex]) +
                    origin + translate;

    gl_Position =
        FLY_ACCESS_UNIFORM_BUFFER(SceneData, gIndices.sceneDataIndex,
                                  projection) *
        FLY_ACCESS_UNIFORM_BUFFER(SceneData, gIndices.sceneDataIndex, view) *
        vec4(worldPos, 1.0f);
}
