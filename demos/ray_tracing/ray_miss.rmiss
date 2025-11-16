#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#include "bindless.glsl"

layout(location = 0) rayPayloadInEXT vec4 payload;

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint sphereBufferIndex;
    uint accStructureIndex;
    uint skyboxTextureIndex;
    uint outputTextureIndex;
    uint sbtStride;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

vec3 Reinhard(vec3 color) { return color / (vec3(1.0f) + color); }

void main()
{
    vec3 skyBoxColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                   Cubemaps, gPushConstants.skyboxTextureIndex),
                               gl_WorldRayDirectionEXT)
                           .rgb;
    vec3 finalColor = Reinhard(skyBoxColor);
    payload = vec4(finalColor, 1.0f);
}
