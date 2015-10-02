#include "shader-common.hlsli"

Texture2D<float3> g_texDiffuse : register(t0);
SamplerState g_ss : register(s0);

void main(
	in Vertex i_vtx,
	in float3 i_vecCamera : CAMERA,
	in float4 i_uvzwShadow : UVZW_SHADOW,
	out float3 o_rgb : SV_Target)
{
	float3 normal = normalize(i_vtx.m_normal);

	// Sample shadow map
	float shadow = EvaluateShadow(i_uvzwShadow, normal);

	// Evaluate diffuse lighting
	float3 diffuseColor = g_texDiffuse.Sample(g_ss, i_vtx.m_uv);
	float3 diffuseLight = g_rgbDirectionalLight * (shadow * saturate(dot(normal, g_vecDirectionalLight)));
	diffuseLight += SimpleAmbient(normal);

	o_rgb = diffuseColor * diffuseLight;
}
