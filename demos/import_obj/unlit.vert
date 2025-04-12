#version 450

layout(location = 0) out vec2 out_uv;

layout(set = 0, binding = 0) uniform CameraMatrices
{
    mat4 projection;
    mat4 view;
}
uniform_data;

struct Vertex
{
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
};

layout(set = 1, binding = 0, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

void main()
{
    Vertex v = vertices[gl_VertexIndex];
    out_uv = vec2(v.uv_x, v.uv_y);
    gl_Position =
        uniform_data.projection * uniform_data.view * vec4(v.position, 1.0f);
}
