#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "bindless.glsl"

layout(location = 0) rayPayloadInEXT vec4 payload;

void main()
{

    vec3 p = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    vec3 n = normalize(p);

    vec3 albedo = vec3(1.0f, 0.0f, 0.0f);
    vec3 l = vec3(0.0f, 1.0f, 0.0f);
    vec3 color = max(dot(n, l), 0.0f) * albedo;
    payload = vec4(color, 1.0f);
}
