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
	// Generate a quad as two tris
	if (iVertex == 0)
	{
		o_posClip = float4(dstMins.x * 2.0 - 1.0, dstMaxs.y * -2.0 + 1.0, 0.0, 1.0);
		o_uv = float2(srcMins.x, srcMaxs.y);
	}
	else if (iVertex == 1 || iVertex == 4)
	{
		o_posClip = float4(dstMaxs.x * 2.0 - 1.0, dstMaxs.y * -2.0 + 1.0, 0.0, 1.0);
		o_uv = float2(srcMaxs.x, srcMaxs.y);
	}
	else if (iVertex == 2 || iVertex == 3)
	{
		o_posClip = float4(dstMins.x * 2.0 - 1.0, dstMins.y * -2.0 + 1.0, 0.0, 1.0);
		o_uv = float2(srcMins.x, srcMins.y);
	}
	else
	{
		o_posClip = float4(dstMaxs.x * 2.0 - 1.0, dstMins.y * -2.0 + 1.0, 0.0, 1.0);
		o_uv = float2(srcMaxs.x, srcMins.y);
	}
}
