#version 450

layout(set = 2, binding = 0) uniform sampler2D diffuse_sampler;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main()
{
    //out_color = vec4(in_uv, 0.0f, 1.0f);
    out_color = texture(diffuse_sampler, in_uv);
}
