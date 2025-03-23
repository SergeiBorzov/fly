#version 450

layout(set = 0, binding = 0) uniform CameraMatrices
{
    mat4 projection;
    mat4 view;
    float time;
}
uniform_data;

layout(location = 0) out vec2 out_uvs;
layout(location = 1) out vec3 out_color;

const vec3 cube_positions[] = vec3[](
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f), vec3(-0.5f, -0.5f, -0.5f), vec3(0.5f, 0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),

    vec3(0.5f, -0.5f, -0.5f), vec3(0.5f, -0.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(0.5f, -0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f), vec3(0.5f, 0.5f, -0.5f),

    vec3(0.5f, -0.5f, 0.5f), vec3(-0.5f, -0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f),
    vec3(0.5f, -0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f),

    vec3(-0.5f, -0.5f, 0.5f), vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(-0.5f, 0.5f, 0.5f),

    vec3(-0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f),

    vec3(-0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, -0.5f),
    vec3(-0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, -0.5f),
    vec3(-0.5f, -0.5f, -0.5f));

const vec2 uvs[] = vec2[](vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f),

                          vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f),

                          vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f),

                          vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f),

                          vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f),

                          vec2(0.0f, 0.0f), vec2(1.0f, 0.0f), vec2(1.0f, 1.0f),
                          vec2(0.0f, 0.0f), vec2(1.0f, 1.0f), vec2(0.0f, 1.0f));

const vec3 face_colors[] = vec3[](
    vec3(1.0f, 1.0f, 1.0f), vec3(1.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 1.0f));

mat3 rotate_y(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, 0.0f, s, 0.0f, 1.0f, 0.0f, -s, 0.0f, c);
}

mat3 rotate_x(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(1.0f, 0.0f, 0.0f, 0.0f, c, -s, 0.0f, s, c);
}

mat3 rotate_z(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f);
}

float rand(float co) { return fract(sin(co * (91.3458)) * 47453.5453); }

void main()
{
    out_uvs = uvs[gl_VertexIndex];
    out_color = face_colors[gl_VertexIndex / 6];

    vec3 origin =
        vec3(gl_InstanceIndex % 100 - 5, gl_InstanceIndex / 100 - 5, 0);
    float r = rand(gl_InstanceIndex);
    vec3 direction =
        vec3(r, rand(gl_InstanceIndex % 7), rand(gl_InstanceIndex % 21)) *
            10.0f -
        vec3(5.0f);
    vec3 translate = direction * uniform_data.time;

    float angle = uniform_data.time * r * 5.0f;
    vec3 world_pos = (rotate_y(angle) * rotate_x(angle) * rotate_z(angle) *
                      cube_positions[gl_VertexIndex]) +
                     origin + translate;

    gl_Position =
        uniform_data.projection * uniform_data.view * vec4(world_pos, 1.0f);
}
