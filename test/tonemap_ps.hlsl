#include "shader-common.h"

Texture2DMS<float3> tex : register(t0);
static const int s_msaaSamples = 4;

void main(
	in float4 pos : SV_Position,
	out float3 o_rgb : SV_Target)
{
	int2 pixelPos = int2(pos.xy);

	// Accumulate all the samples in the pixel.
	// For now, just average them (box filter)...later could do a Gaussian.
	float3 rgb = 0.0.xxx;
	[unroll] for (int i = 0; i < s_msaaSamples; ++i)
	{
		rgb += tex.Load(pixelPos, i);
	}
	rgb *= (1.0 / float(s_msaaSamples));

	// Apply tonemapping to the final color, using Jim Hejl's 2015 curve
	rgb *= g_exposure;
	float3 rgb2 = (1.425 * rgb) + 0.05;
	o_rgb = ((rgb * rgb2 + 0.004) / ((rgb * (rgb2 + 0.55) + 0.0491))) - 0.0821;
}
