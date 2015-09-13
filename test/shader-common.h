#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

#include "shader-slots.h"

#pragma pack_matrix(row_major)

struct Vertex
{
	float3		m_pos		: POSITION;
	float3		m_normal	: NORMAL;
	float2		m_uv		: UV;
};

cbuffer CBFrame : CB_FRAME					// matches struct CBFrame in test.cpp
{
	float4x4	g_matWorldToClip;
	float4x4	g_matWorldToUvzwShadow;
	float3x3	g_matWorldToUvzShadowNormal;
	float3		g_posCamera;

	float3		g_vecDirectionalLight;
	float3		g_rgbDirectionalLight;

	float		g_shadowSharpening;
	float3		g_shadowFilterUVZScale;
	float		g_normalOffsetShadow;
	float2		g_dimsShadowMap;

	float		g_exposure;					// Exposure multiplier
}

cbuffer CBDebug : CB_DEBUG					// matches struct CBDebug in test.cpp
{
	float		g_debugKey;					// Mapped to spacebar - 0 if up, 1 if down
	float		g_debugSlider0;				// Mapped to debug sliders in UI
	float		g_debugSlider1;				// ...
	float		g_debugSlider2;				// ...
	float		g_debugSlider3;				// ...
}

float square(float x) { return x*x; }
float2 square(float2 x) { return x*x; }
float3 square(float3 x) { return x*x; }
float4 square(float4 x) { return x*x; }

float sharpen(float x, float sharpening)
{
	if (x < 0.5)
		return 0.5 * pow(2.0*x, sharpening);
	else
		return -0.5 * pow(-2.0*x + 2.0, sharpening) + 1.0;
}



// Simple ramp ambient
float3 SimpleAmbient(float3 normal)
{
	float3 skyColor = { 0.09, 0.11, 0.2 };
	float3 groundColor = { 0.15, 0.15, 0.15 };
	float3 sideColor = { 0.03, 0.02, 0.01 };
	return lerp(groundColor, skyColor, normal.y * 0.5 + 0.5) + sideColor * square(saturate(normal.z));
}



// PCF shadow filtering

Texture2D<float> g_texShadowMap : TEX_SHADOW;
SamplerComparisonState g_ssShadow : SAMP_SHADOW;

// Poisson disk generated with http://www.coderhaus.com/?p=11
static const float2 s_aPoisson8[] = 
{ 	
	{ -0.7494944f, 0.1827986f, },
	{ -0.8572887f, -0.4169083f, },
	{ -0.1087135f, -0.05238153f, },
	{ 0.1045462f, 0.9657645f, },
	{ -0.0135659f, -0.698451f, },
	{ -0.4942278f, 0.7898396f, },
	{ 0.7970678f, -0.4682421f, },
	{ 0.8084122f, 0.533884f },
};

float EvaluateShadowPoisson8(
	float4 uvzwShadow,
	float3 normalGeom)
{
	float3 uvzShadow = uvzwShadow.xyz / uvzwShadow.w;

	// Calculate surface slope wrt shadow map UV, for tilting the shadow sampling pattern
	float3 normalShadow = mul(normalGeom, g_matWorldToUvzShadowNormal);
	float2 uvSlopes = -normalShadow.xy / normalShadow.z;

	// Apply normal offset to avoid self-shadowing artifacts
	uvzShadow += normalShadow * g_normalOffsetShadow;

	float2 filterScale = g_shadowFilterUVZScale.xy;

	// Reduce filter width if it's tilted too far, to respect a maximum Z offset
	float maxZOffset = length(uvSlopes * filterScale);
	if (maxZOffset > g_shadowFilterUVZScale.z)
	{
		filterScale *= g_shadowFilterUVZScale.z / maxZOffset;
	}

	// Do the samples - each one a bilinear 2x2 PCF tap
	float sampleSum = 0.0;
	[unroll] for (int i = 0; i < 8; ++i)
	{
		float2 uvDelta = s_aPoisson8[i] * filterScale;
		float2 uvSample = uvzShadow.xy + uvDelta;
		float zSample = uvzShadow.z + dot(uvSlopes, uvDelta);
		sampleSum += g_texShadowMap.SampleCmp(g_ssShadow, uvSample, zSample);
	}

	return sharpen(sampleSum * (1.0 / 8.0), g_shadowSharpening);
}

float EvaluateShadowGather16(
	float4 uvzwShadow,
	float3 normalGeom)
{
	float3 uvzShadow = uvzwShadow.xyz / uvzwShadow.w;

	// Apply normal offset to avoid self-shadowing artifacts
	float3 normalShadow = mul(normalGeom, g_matWorldToUvzShadowNormal);
	uvzShadow += normalShadow * g_normalOffsetShadow;

	// Do the samples - each one a 2x2 GatherCmp
	// !!!UNDONE: not offsetting the z value by uvSlopes
	float4 samplesNW = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2(-1, -1));
	float4 samplesNE = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2( 1, -1));
	float4 samplesSW = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2(-1,  1));
	float4 samplesSE = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2( 1,  1));

	// Calculate fractional location relative to texel centers.  The 1/512 offset is needed to ensure
	// that frac()'s output steps from 1 to 0 at the exact same point that GatherCmp switches texels.
	float2 offset = frac(uvzShadow.xy * g_dimsShadowMap + (-0.5 + 1.0/512.0));

	// Calculate weights for the samples based on a 2px-radius triangle filter
	static const float radius = 2.0, invR = 1.0 / radius, invRSq = 1.0 / square(radius);
	// Readable version: float4 xWeights = { 1.0 - (offset+1)/radius, 1.0 - offset/radius, 1.0 - (1-offset)/radius, 1.0 - (2-offset)/radius } / radius;
	float4 xWeights = offset.x * float4(-invRSq.xx, invRSq.xx) + (invR + float4(-invRSq, 0.0, -invRSq, -2.0*invRSq));
	float4 yWeights = offset.y * float4(-invRSq.xx, invRSq.xx) + (invR + float4(-invRSq, 0.0, -invRSq, -2.0*invRSq));

	// Calculate weighted sum of samples
	float sampleSum = dot(xWeights.xyyx, yWeights.yyxx * samplesNW) +
					  dot(xWeights.zwwz, yWeights.yyxx * samplesNE) +
					  dot(xWeights.xyyx, yWeights.wwzz * samplesSW) +
					  dot(xWeights.zwwz, yWeights.wwzz * samplesSE);

	return sharpen(sampleSum, g_shadowSharpening);
}

float EvaluateShadow(
	float4 uvzwShadow,
	float3 normalGeom)
{
	if (g_debugKey)
		return EvaluateShadowGather16(uvzwShadow, normalGeom);
	else
		return EvaluateShadowPoisson8(uvzwShadow, normalGeom);
}

#endif // !defined(SHADER_COMMON_H)
