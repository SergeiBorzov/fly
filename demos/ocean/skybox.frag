#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 faceColors[6] = vec3[](vec3(1.0, 0.0, 0.0), // +X
                                vec3(0.0, 1.0, 0.0), // -X
                                vec3(0.0, 0.0, 1.0), // +Y
                                vec3(1.0, 1.0, 0.0), // -Y
                                vec3(1.0, 0.0, 1.0), // +Z
                                vec3(0.0, 1.0, 1.0)  // -Z
    );

    outColor = vec4(faceColors[gl_ViewIndex], 1.0);
}
