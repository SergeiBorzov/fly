#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint splatBufferIndex;
}
gPushConstants;

FLY_REGISTER_UNIFORM_BUFFER(Camera, {
    mat4 projection;
    mat4 view;
    vec4 viewport;
    vec4 cameraParameters;
    vec4 time;
})

FLY_REGISTER_STORAGE_BUFFER(readonly, Splat, {
    vec4 rotation;
    vec3 position;
    float r;
    vec3 scale;
    float g;
    float b;
    float a;
    float pad[2];
})

mat3 rotMat3(vec4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    return mat3(1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz), 2.0f * (xz + wy),
                2.0f * (xy + wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),
                2.0f * (xz - wy), 2.0f * (yz + wx), 1.0f - 2.0f * (xx + yy));
}

mat3 scaleMat3(vec3 scale)
{
    return mat3(scale.x, 0.0f, 0.0f, 0.0f, scale.y, 0.0f, 0.0f, 0.0f, scale.z);
}

const vec2 positions[6] =
    vec2[6](vec2(-2.0f, -2.0f), vec2(2.0f, -2.0f), vec2(-2.0f, 2.0f),
            vec2(-2.0f, 2.0f), vec2(2.0f, -2.0f), vec2(2.0f, 2.0f));

#define MAX_SPLAT_SIZE 128.0f

void main()
{
    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, view);
    vec4 viewport = FLY_ACCESS_UNIFORM_BUFFER(
        Camera, gPushConstants.cameraBufferIndex, viewport);

    Splat splat = FLY_ACCESS_STORAGE_BUFFER(
        Splat, gPushConstants.splatBufferIndex)[gl_InstanceIndex];

    vec4 q = normalize(vec4(splat.rotation.x, splat.rotation.y,
                            splat.rotation.z, splat.rotation.w));
    mat3 rs = rotMat3(q) * scaleMat3(splat.scale);

    vec4 viewPos = view * vec4(splat.position, 1.0f);
    vec4 clipPos = projection * viewPos;

    mat3 cov = rs * transpose(rs);
    // last column can be zeros, we don't care about z
    // since we just render back to front with blending enabled
    float fy = 1164.6601287484507;
    float fx = 1159.5880733038064;

    float f = -projection[1][1] * viewport.y * 0.5f;
    float z2 = viewPos.z * viewPos.z;
    mat3 j = mat3(f / viewPos.z, 0.0f, 0.0f, 0.0f, -f / viewPos.z, 0.0f,
                  -(f * viewPos.x) / z2, (f * viewPos.y) / z2, 0.0f);
    mat3 t = j * mat3(view);
    mat3 cov2d = t * cov * transpose(t);

    // Note: covariance matrix is symmetric
    float a = cov2d[0][0];
    float b = cov2d[0][1];
    float d = cov2d[1][1];
    float det = a * d - b * b;
    float ht = (a + d) * 0.5f; // half of trace
    float disc = sqrt(ht * ht - det);

    float e1 = ht + disc;
    float e2 = ht - disc;

    if (e2 < 0.0)
    {
        return;
    }

    vec2 v1 = normalize(vec2(b, e1 - a));
    vec2 v2 = vec2(v1.y, -v1.x);

    // Note:
    // 2.0f multiplier covers 95.4% of total data
    // 4.0f multiplie covers 99.99% of total data
    vec2 s1 = v1 * min(sqrt(e1) * 3.0f, MAX_SPLAT_SIZE);
    vec2 s2 = v2 * min(sqrt(e2) * 3.0f, MAX_SPLAT_SIZE);

    vec3 ndcPos = clipPos.xyz / clipPos.w;

    vec2 quadPos = positions[gl_VertexIndex];
    gl_Position = vec4(ndcPos.xy + quadPos.x * s1 * viewport.zw +
                           quadPos.y * s2 * viewport.zw,
                       ndcPos.z, 1.0f);

    outUV = quadPos;
    vec3 linear = pow(vec3(splat.r, splat.g, splat.b), vec3(2.2));
    outColor = vec4(linear, splat.a);
}
