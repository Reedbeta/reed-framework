#include "shader-common.h"

Texture2D<float4> g_texDiffuse : register(t0);
SamplerState g_ss : register(s0);

void main(in Vertex i_vtx)
{
	float4 diffuseColor = g_texDiffuse.Sample(g_ss, i_vtx.m_uv);
	if (diffuseColor.a < 0.5)
		discard;
}
