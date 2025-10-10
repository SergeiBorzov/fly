#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable
#include "bindless.glsl"

#define PI 3.14159265359f
#define GOLDEN_RATIO 1.6180339f

layout(location = 0) in vec3 inDirection;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint radianceMapIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemap, samplerCube)

mat3 FaceRotation(uint faceIndex)
{
    const vec3 faceTargets[6] = vec3[](vec3(1, 0, 0),  // +X
                                       vec3(-1, 0, 0), // -X
                                       vec3(0, 1, 0),  // +Y
                                       vec3(0, -1, 0), // -Y
                                       vec3(0, 0, 1),  // +Z
                                       vec3(0, 0, -1)  // -Z
    );

    const vec3 upVectors[6] =
        vec3[](vec3(0, 1, 0), vec3(0, 1, 0), vec3(0, 0, -1), vec3(0, 0, 1),
               vec3(0, 1, 0), vec3(0, 1, 0));

    vec3 f = faceTargets[faceIndex];
    vec3 u = upVectors[faceIndex];
    vec3 r = normalize(cross(u, f)); // ensures right-handed basis
    u = normalize(cross(f, r));
    return mat3(r, u, f);
}

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

#define SAMPLE_COUNT 32

void main()
{
    mat3 r = FaceRotation(gl_ViewIndex);
    vec3 normal = normalize(r * inDirection);

    vec3 irradiance = vec3(0.0f);
    float delta = PI / float(SAMPLE_COUNT);
    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {
        float fi = float(i) + 0.5f;
        float phi = acos(1.0f - fi / float(SAMPLE_COUNT));
        float theta = PI * GOLDEN_RATIO * fi;

        float x = sin(phi) * cos(theta);
        float y = cos(phi);
        float z = sin(phi) * sin(theta);
        vec3 localDir = vec3(x, y, z);

        vec3 worldUp = abs(normal.y) < 0.999 ? vec3(0.0f, 1.0f, 0.0f)
                                             : vec3(1.0f, 0.0f, 0.0f);
        vec3 tangent = normalize(cross(worldUp, normal));
        vec3 bitangent = normalize(cross(normal, tangent));

        vec3 sampleDir =
            localDir.x * tangent + localDir.y * normal + localDir.z * bitangent;

        irradiance += delta *
                      texture(FLY_ACCESS_TEXTURE_BUFFER(
                                  Cubemap, gPushConstants.radianceMapIndex),
                              sampleDir)
                          .rgb *
                      cos(phi) * sin(phi);
    }

    outFragColor = vec4(Reinhard(irradiance, 1.0f), 1.0f);
}
