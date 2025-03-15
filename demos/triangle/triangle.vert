#version 450

layout(location = 0) out vec3 outColor;

const vec3 positions[3] = vec3[3](
    vec3(0.0f, -0.5f, 0.0f), vec3(0.5f, 0.5f, 0.0f), vec3(-0.5f, 0.5f, 0.0f));

const vec3 colors[3] = vec3[3](vec3(1.0f, 0.0f, 0.0f), // red
                               vec3(0.0f, 1.0f, 0.0f), // green
                               vec3(00.f, 0.0f, 1.0f)  // blue
);

void main()
{
    outColor = colors[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
}
