#version 460
#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"

layout(location = 0) in vec3 inDirection;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

vec3 Reinhard(vec3 color) { return color / (vec3(1.0f) + color); }

void main()
{
    vec3 dir = normalize(inDirection);
    vec3 skyBoxColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                   Cubemaps, gPushConstants.skyBoxTextureIndex),
                               dir)
                           .rgb;
    vec3 finalColor = Reinhard(skyBoxColor);
    //gl_FragDepth = 0.0f;
    outFragColor = vec4(finalColor, 1.0f);
}
