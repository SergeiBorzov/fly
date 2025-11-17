#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#include "bindless.glsl"
#include "common.glsl"

layout(location = 0) rayPayloadInEXT Payload payload;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

void main()
{
    vec3 skyBoxColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                   Cubemaps, gPushConstants.skyboxTextureIndex),
                               gl_WorldRayDirectionEXT)
                           .rgb;
    payload.done = 1;
    payload.luminance = payload.throughput * skyBoxColor;
}
