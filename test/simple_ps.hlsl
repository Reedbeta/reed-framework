#include "shader-common.h"

Texture2D<float3> g_texDiffuse : register(t0);
SamplerState g_ss : register(s0);

void main(
	in Vertex i_vtx,
	in float3 i_vecCamera : CAMERA,
	in float4 i_uvzwShadow : UVZW_SHADOW,
	out float3 o_rgb : SV_Target)
{
	float3 normal = normalize(i_vtx.m_normal);

	float3 diffuseColor = g_texDiffuse.Sample(g_ss, i_vtx.m_uv);
	float3 diffuseLight = g_rgbDirectionalLight * saturate(dot(normal, g_vecDirectionalLight));

	// Simple ramp ambient
	float3 skyColor = { 0.09, 0.11, 0.2 };
	float3 groundColor = { 0.15, 0.15, 0.15 };
	float3 sideColor = { 0.03, 0.02, 0.01 };
	diffuseLight += lerp(groundColor, skyColor, normal.y * 0.5 + 0.5);
	diffuseLight += sideColor * square(saturate(normal.z));

	o_rgb = diffuseColor * diffuseLight;
}
