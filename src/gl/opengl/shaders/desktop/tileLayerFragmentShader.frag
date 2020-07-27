#version 330
#extension GL_ARB_separate_shader_objects : require

uniform sampler2D SPIRV_Cross_CombinedcolourTexturecolourSampler;

layout(location = 0) in vec4 in_var_COLOR0;
layout(location = 1) in vec2 in_var_TEXCOORD0;
layout(location = 0) out vec4 out_var_SV_Target0;
layout(location = 1) out vec4 out_var_SV_Target1;

void main()
{
    vec4 _31 = texture(SPIRV_Cross_CombinedcolourTexturecolourSampler, in_var_TEXCOORD0);
    out_var_SV_Target0 = vec4(_31.xyz * in_var_COLOR0.xyz, _31.w * in_var_COLOR0.w);
    out_var_SV_Target1 = vec4(0.0);
}

