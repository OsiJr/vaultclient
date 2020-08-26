#version 330
#extension GL_ARB_separate_shader_objects : require

out gl_PerVertex
{
    vec4 gl_Position;
};

struct InstanceData
{
    mat4 worldViewProjection;
    vec4 colour;
    vec4 objectInfo;
};

layout(std140) uniform type_u_EveryInstanceDynamic
{
    layout(row_major) InstanceData u_instanceData[682];
} u_EveryInstanceDynamic;

layout(location = 0) in vec3 in_var_POSITION;
layout(location = 1) in vec3 in_var_NORMAL;
layout(location = 2) in vec2 in_var_TEXCOORD0;
uniform int SPIRV_Cross_BaseInstance;
layout(location = 0) out vec2 out_var_TEXCOORD0;
layout(location = 1) out vec3 out_var_NORMAL;
layout(location = 2) out vec4 out_var_COLOR0;
layout(location = 3) out vec2 out_var_TEXCOORD1;
layout(location = 4) out vec2 out_var_TEXCOORD2;

void main()
{
    vec4 _53 = vec4(in_var_POSITION, 1.0) * u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].worldViewProjection;
    vec2 _68;
    switch (0u)
    {
        default:
        {
            if (u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].worldViewProjection[3].y == 0.0)
            {
                _68 = vec2(_53.zw);
                break;
            }
            _68 = vec2(1.0 + _53.w, 0.0);
            break;
        }
    }
    gl_Position = _53;
    out_var_TEXCOORD0 = in_var_TEXCOORD0;
    out_var_NORMAL = vec3(0.0);
    out_var_COLOR0 = u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].colour;
    out_var_TEXCOORD1 = _68;
    out_var_TEXCOORD2 = u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].objectInfo.xy;
}

