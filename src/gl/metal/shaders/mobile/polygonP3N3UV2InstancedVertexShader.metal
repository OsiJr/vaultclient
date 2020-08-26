#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct InstanceData
{
    float4x4 worldViewProjection;
    float4 colour;
    float4 objectInfo;
};

struct type_u_EveryInstanceDynamic
{
    InstanceData u_instanceData[682];
};

struct main0_out
{
    float2 out_var_TEXCOORD0 [[user(locn0)]];
    float3 out_var_NORMAL [[user(locn1)]];
    float4 out_var_COLOR0 [[user(locn2)]];
    float2 out_var_TEXCOORD1 [[user(locn3)]];
    float2 out_var_TEXCOORD2 [[user(locn4)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 in_var_POSITION [[attribute(0)]];
    float2 in_var_TEXCOORD0 [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant type_u_EveryInstanceDynamic& u_EveryInstanceDynamic [[buffer(0)]], uint gl_InstanceIndex [[instance_id]])
{
    main0_out out = {};
    float4 _53 = u_EveryInstanceDynamic.u_instanceData[gl_InstanceIndex].worldViewProjection * float4(in.in_var_POSITION, 1.0);
    float2 _68;
    switch (0u)
    {
        default:
        {
            if (u_EveryInstanceDynamic.u_instanceData[gl_InstanceIndex].worldViewProjection[1][3] == 0.0)
            {
                _68 = float2(_53.zw);
                break;
            }
            _68 = float2(1.0 + _53.w, 0.0);
            break;
        }
    }
    out.gl_Position = _53;
    out.out_var_TEXCOORD0 = in.in_var_TEXCOORD0;
    out.out_var_NORMAL = float3(0.0);
    out.out_var_COLOR0 = u_EveryInstanceDynamic.u_instanceData[gl_InstanceIndex].colour;
    out.out_var_TEXCOORD1 = _68;
    out.out_var_TEXCOORD2 = u_EveryInstanceDynamic.u_instanceData[gl_InstanceIndex].objectInfo.xy;
    return out;
}

