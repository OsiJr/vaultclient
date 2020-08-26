#version 300 es

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
out vec2 varying_TEXCOORD0;
out vec3 varying_NORMAL;
out vec4 varying_COLOR0;
out vec2 varying_TEXCOORD1;
out vec2 varying_TEXCOORD2;

void main()
{
    vec4 _53 = vec4(in_var_POSITION, 1.0) * u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].worldViewProjection;
    vec2 _68;
    switch (0u)
    {
        case 0u:
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
    varying_TEXCOORD0 = in_var_TEXCOORD0;
    varying_NORMAL = vec3(0.0);
    varying_COLOR0 = u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].colour;
    varying_TEXCOORD1 = _68;
    varying_TEXCOORD2 = u_EveryInstanceDynamic.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].objectInfo.xy;
}

