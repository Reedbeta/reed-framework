cbuffer CBBlit : register(b0)	// Matches struct CBBlit in d3d11-window.h
{
	float2	srcMins, srcMaxs;	// Source rect in UV space
	float2	dstMins, dstMaxs;	// Dest rect in UV space
}

void main(
	in uint iVertex : SV_VertexID,
	out float4 o_posClip : SV_Position,
	out float2 o_uv : UV)
{
	// Generate a full-screen triangle
	if (iVertex == 0)
	{
		o_posClip = float4(-1.0, -1.0, 0.0, 1.0);
		o_uv = float2(srcMins.x, srcMaxs.y);
	}
	else if (iVertex == 1)
	{
		o_posClip = float4(3.0, -1.0, 0.0, 1.0);
		o_uv = float2(2.0 * srcMaxs.x - srcMins.x, srcMaxs.y);
	}
	else
	{
		o_posClip = float4(-1.0, 3.0, 0.0, 1.0);
		o_uv = float2(srcMins.x, 2.0 * srcMins.y - srcMaxs.y);
	}
}
