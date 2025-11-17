#define MAX_BOUNCES 3
#define PI 3.14159265359f
#define EPS 0.05f

layout(push_constant) uniform PushConstants
{
    uint cameraBufferIndex;
    uint sphereBufferIndex;
    uint accStructureIndex;
    uint skyboxTextureIndex;
    uint outputTextureIndex;
    uint sbtStride;
    uint currentSample;
    uint sampleCount;
}
gPushConstants;

struct Payload
{
    vec3 throughput;
    vec3 luminance;
    vec3 p;
    vec3 n;
    float reflectionCoeff;
    uint seed;
    uint maxSamples;
    int done;
    int depth;
};
