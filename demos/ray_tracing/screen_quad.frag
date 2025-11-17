#version 460
#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants { uint outputTextureIndex; }
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

vec3 Reinhard(vec3 color) { return color / (vec3(1.0f) + color); }

void main()
{
    vec3 color = texture(FLY_ACCESS_TEXTURE_BUFFER(
                             Textures, gPushConstants.outputTextureIndex),
                         inUV)
                     .rgb;
    outFragColor = vec4(Reinhard(color), 1.0f);
}
