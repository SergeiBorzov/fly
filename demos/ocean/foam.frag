#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants { uint previousFoamTextureIndex; }
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

void main()
{
    vec3 foamColor =
        texture(FLY_ACCESS_TEXTURE_BUFFER(
                    Textures, gPushConstants.previousFoamTextureIndex),
                inUV).rgb;
    vec3 finalColor = foamColor + vec3(0.004f, 0.0f, 0.0f);
    outFragColor = vec4(finalColor, 1.0f);
}
