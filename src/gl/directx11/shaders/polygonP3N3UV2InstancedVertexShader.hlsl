cbuffer u_cameraPlaneParams
{
  float s_CameraNearPlane;
  float s_CameraFarPlane;
  float u_clipZNear;
  float u_clipZFar;
};

struct VS_INPUT
{
  float3 pos : POSITION;
  float3 normal : NORMAL;
  float2 uv  : TEXCOORD0;
  //float4x4 instanceModel : TEXCOORD1;
  uint instanceId : SV_InstanceID;
};

struct PS_INPUT
{
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
  float3 normal : NORMAL;
  float4 colour : COLOR0;
  float2 depthInfo : TEXCOORD1;
  float2 objectInfo : TEXCOORD2;
};

struct InstanceData
{
  float4x4 worldViewProjection;
  float4 colour;
  float4 objectInfo; // id.x, isSelectable.y, (unused).zw
};

//cbuffer u_EveryInstanceStatic : register(b0)
//{
//  InstanceData u_instanceData[682];
//};
//

cbuffer u_EveryInstanceDynamic : register(b0)
{
  InstanceData u_instanceData[682];
};

//cbuffer u_EveryFrame : register(b2)
//{
//  float4x4 u_viewProjectionMatrix;
//};

float2 PackDepth(float4 clipPos, float4x4 worldViewProjection)
{
  if (worldViewProjection[3][1] == 0.0) // orthographic
    return float2(clipPos.z, clipPos.w);

  // perspective
  return float2(1.0 + clipPos.w, 0);
}

PS_INPUT main(VS_INPUT input)
{
  PS_INPUT output;

  // making the assumption that the model matrix won't contain non-uniform scale
  float3 worldNormal = float3(0,0,0);//normalize(mul(u_instanceData[input.instanceId].worldMatrix, float4(input.normal, 0.0)).xyz);

  float4x4 mvp = u_instanceData[input.instanceId].worldViewProjection;//mul(u_viewProjectionMatrix, u_instanceData[input.instanceId].worldMatrix);
  output.pos = mul(mvp, float4(input.pos, 1.0));
  output.uv = input.uv;
  output.normal = worldNormal;
  output.colour = u_instanceData[input.instanceId].colour;// * input.colour;
  output.depthInfo = PackDepth(output.pos, u_instanceData[input.instanceId].worldViewProjection);
  output.objectInfo = u_instanceData[input.instanceId].objectInfo.xy;

  return output;
}
