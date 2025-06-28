#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 GetViewDirection(uint faceIndex, vec2 uv)
{
    vec2 coord = uv * 2.0f - 1.0f;

    if (faceIndex == 0)
    {
        return normalize(vec3(1.0f, -coord.y, -coord.x));
    }
    else if (faceIndex == 1)
    {
        return normalize(vec3(-1.0f, -coord.y, coord.x));
    }
    else if (faceIndex == 2)
    {
        return normalize(vec3(coord.x, -1.0f, coord.y));
    }
    else if (faceIndex == 3)
    {
        return normalize(vec3(coord.x, 1.0f, -coord.y));
    }
    else if (faceIndex == 4)
    {
        return normalize(vec3(coord.x, -coord.y, 1.0f));
    }
    else
    {
        return normalize(vec3(-coord.x, -coord.y, -1.0f));
    }
}

void main()
{
    vec3 dir = GetViewDirection(gl_ViewIndex, uv);

    float t = clamp(dir.y * 0.5f + 0.5f, 0.0f, 1.0f);

    // vec3 bottomColor = vec3(0.102f, 0.173f, 0.302f);
    // vec3 topColor = vec3(0.039f, 0.059f, 0.173f);

    vec3 bottomColor = vec3(0.16f, 0.30f, 0.55f);
    vec3 topColor = vec3(0.08f, 0.15f, 0.45f);

    vec3 gradient = mix(bottomColor, topColor, t);

    outColor = vec4(gradient, 1.0f);
}
