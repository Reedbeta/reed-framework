#include "shader-common.hlsli"

void main(
	in Vertex i_vtx,
	out Vertex o_vtx,
	out float3 o_vecCamera : CAMERA,
	out float4 o_uvzwShadow : UVZW_SHADOW,
	out float4 o_posClip : SV_Position)
{
	o_vtx = i_vtx;
	o_vecCamera = g_posCamera - i_vtx.m_pos;
	o_uvzwShadow = mul(float4(i_vtx.m_pos, 1.0), g_matWorldToUvzwShadow);
	o_posClip = mul(float4(i_vtx.m_pos, 1.0), g_matWorldToClip);
}
