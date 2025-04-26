#version 450

#extension GL_EXT_nonuniform_qualifier : enable

struct Vertex
{
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
};

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform Indices
{
    uint cameraIndex;
    uint vertexBufferIndex;
    uint albedoTextureIndex;
} uIndices;

layout(set = 0, binding = 0) uniform CameraMatrices
{
    mat4 projection;
    mat4 view;
} uCameras[];

layout(set = 0, binding = 1, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
} uVertexBuffers[];

void main()
{
    Vertex v = uVertexBuffers[uIndices.vertexBufferIndex].vertices[gl_VertexIndex];
    outUV = vec2(v.uvX, v.uvY);
    gl_Position = uCameras[uIndices.cameraIndex].projection *
                  uCameras[uIndices.cameraIndex].view * vec4(v.position, 1.0f);
}
