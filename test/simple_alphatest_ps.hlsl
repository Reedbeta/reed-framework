#include "shader-common.h"

Texture2D<float4> g_texDiffuse : register(t0);
SamplerState g_ss : register(s0);

void main(
	in Vertex i_vtx,
	in float3 i_vecCamera : CAMERA,
	in float4 i_uvzwShadow : UVZW_SHADOW,
	in bool i_isFrontFace : SV_IsFrontFace,
	out float3 o_rgb : SV_Target)
{
	float3 normal = normalize(i_vtx.m_normal) * (i_isFrontFace ? 1.0 : -1.0);

	float4 diffuseColor = g_texDiffuse.Sample(g_ss, i_vtx.m_uv);
	if (diffuseColor.a < 0.5)
		discard;

	// Sample shadow map
	float shadow = EvaluateShadowPCF8(i_uvzwShadow, normal);

	// Evaluate diffuse lighting
	float3 diffuseLight = g_rgbDirectionalLight * (shadow * saturate(dot(normal, g_vecDirectionalLight)));
	diffuseLight += SimpleAmbient(normal);

	o_rgb = diffuseColor.rgb * diffuseLight;
}
