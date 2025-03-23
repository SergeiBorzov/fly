#version 450

layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(tex_sampler, in_uvs) * vec4(in_color, 1.0f);
}
