#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 GetViewDirection(uint faceIndex, vec2 uv)
{
    vec2 coord = uv * 2.0 - 1.0;

    if (faceIndex == 0)
    {
        return normalize(vec3(1.0, -coord.y, -coord.x));
    }
    else if (faceIndex == 1)
    {
        return normalize(vec3(-1.0, -coord.y, coord.x));
    }
    else if (faceIndex == 2)
    {
        return normalize(vec3(coord.x, -1.0, coord.y));
    }
    else if (faceIndex == 3)
    {
        return normalize(vec3(coord.x, 1.0, -coord.y));
    }
    else if (faceIndex == 4)
    {
        return normalize(vec3(coord.x, -coord.y, 1.0));
    }
    else
    {
        return normalize(vec3(-coord.x, -coord.y, -1.0));
    }
}

void main()
{
    vec3 dir = GetViewDirection(gl_ViewIndex, uv);

    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    vec3 bottomColor = vec3(0.1, 0.3, 0.9);
    vec3 topColor = vec3(0.3, 0.6, 1.0);

    vec3 gradient = mix(bottomColor, topColor, t);

    outColor = vec4(gradient, 1.0);
}
