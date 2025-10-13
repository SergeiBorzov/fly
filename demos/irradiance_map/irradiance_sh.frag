#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable
#include "bindless.glsl"

#define PI 3.14159265359f
#define GOLDEN_RATIO 1.6180339f

#define SH_00_SCALE sqrt(1 / (4 * PI))

layout(location = 0) in vec3 inDirection;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint radianceProjectionBufferIndex;
    uint radianceMapIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemap, samplerCube)
FLY_REGISTER_STORAGE_BUFFER(readonly, RadianceProjectionSH, {
    vec3 coefficient;
    float pad;
})

void SH9(vec3 n, out float sh[9])
{
    const float c0 = sqrt(1.0f / (4.0f * PI));
    const float c1 = sqrt(3.0f / (4.0f * PI));
    const float c2 = sqrt(15.0f / (4.0f * PI));
    const float c3 = sqrt(5.0f / (16.0f * PI));
    const float c4 = sqrt(15.0f / (16.0f * PI));

    sh[0] = c0;
    sh[1] = c1 * n.y;
    sh[2] = c1 * n.z;
    sh[3] = c1 * n.x;
    sh[4] = c2 * n.x * n.y;
    sh[5] = c2 * n.y * n.z;
    sh[6] = c3 * (3.0f * n.z * n.z - 1.0f);
    sh[7] = c2 * n.x * n.z;
    sh[8] = c4 * (n.x * n.x - n.y * n.y);
}

vec3 IrradianceSH9(vec3 n, vec3 l[9])
{
    const float c1 = 0.429043f;
    const float c2 = 0.511664f;
    const float c3 = 0.743125f;
    const float c4 = 0.886227f;
    const float c5 = 0.247708f;

    float sh[9];
    SH9(n, sh);

    vec3 m00 = c1 * l[8];
    vec3 m01 = c1 * l[4];
    vec3 m02 = c1 * l[7];
    vec3 m03 = c2 * l[3];
    vec3 m11 = -c1 * l[4];
    vec3 m12 = c1 * l[5];
    vec3 m13 = c2 * l[1];
    vec3 m22 = c3 * l[6];
    vec3 m23 = c2 * l[2];
    vec3 m33 = c4 * l[0] - c5 * l[6];

    mat4 Mr = mat4(
        vec4(m00.r, m01.r, m02.r, m03.r), vec4(m01.r, m11.r, m12.r, m13.r),
        vec4(m02.r, m12.r, m22.r, m23.r), vec4(m03.r, m13.r, m23.r, m33.r));
    mat4 Mg = mat4(
        vec4(m00.g, m01.g, m02.g, m03.g), vec4(m01.g, m11.g, m12.g, m13.g),
        vec4(m02.g, m12.g, m22.g, m23.g), vec4(m03.g, m13.g, m23.g, m33.g));
    mat4 Mb = mat4(
        vec4(m00.b, m01.b, m02.b, m03.b), vec4(m01.b, m11.b, m12.b, m13.b),
        vec4(m02.b, m12.b, m22.b, m23.b), vec4(m03.b, m13.b, m23.b, m33.b));

    vec4 n4 = vec4(n, 1.0f);
    return vec3(dot(n4, Mr * n4), dot(n4, Mg * n4), dot(n4, Mb * n4));
}

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

void main()
{
    mat3 r = FaceRotation(gl_ViewIndex);
    vec3 n = normalize(r * inDirection);

    FLY_ACCESS_STORAGE_BUFFER(RadianceProjectionSH,
                              gPushConstants.radianceProjectionBufferIndex);

    vec3 l[9];
    for (uint i = 0; i < 9; i++)
    {
        l[i] = FLY_ACCESS_STORAGE_BUFFER(
                   RadianceProjectionSH,
                   gPushConstants.radianceProjectionBufferIndex)[i]
                   .coefficient;
    }
    vec3 irradiance = IrradianceSH9(n, l);

    outFragColor = vec4(irradiance, 1.0f);
}
