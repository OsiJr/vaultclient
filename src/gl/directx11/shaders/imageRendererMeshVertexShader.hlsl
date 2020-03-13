cbuffer u_cameraPlaneParams
{
  float s_CameraNearPlane;
  float s_CameraFarPlane;
  float u_unused1;
  float u_unused2;
};

struct VS_INPUT
{
  float3 pos : POSITION;
  float3 normal : NORMAL; // unused
  float2 uv  : TEXCOORD0;
};

struct PS_INPUT
{
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
  float4 colour : COLOR0;
  float2 fLogDepth : TEXCOORD1;
};

cbuffer u_EveryObject : register(b0)
{
  float4x4 u_worldViewProjectionMatrix;
  float4 u_colour;
  float4 u_screenSize; // unused
};

PS_INPUT main(VS_INPUT input)
{
  PS_INPUT output;

  output.pos = mul(u_worldViewProjectionMatrix, float4(input.pos, 1.0));
  output.uv = input.uv;
  output.colour = u_colour;
  output.fLogDepth.x = 1.0 + output.pos.w;

  return output;
}
