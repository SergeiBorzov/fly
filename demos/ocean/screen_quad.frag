#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants { uint textureIndex; }
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Texture, sampler2D)

void main()
{
    float value =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Texture, gPushConstants.textureIndex),
                inUV)
            .r;
    outFragColor = vec4(value, value, value, 1.0f);
}
