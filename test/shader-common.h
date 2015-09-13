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

	float2		g_dimsShadowMap;
	float		g_normalOffsetShadow;
	float		g_shadowSharpening;

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

float EvaluateShadowGather16(
	float4 uvzwShadow,
	float3 normalGeom)
{
	float3 uvzShadow = uvzwShadow.xyz / uvzwShadow.w;

	// Apply normal offset to avoid self-shadowing artifacts
	float3 normalShadow = mul(normalGeom, g_matWorldToUvzShadowNormal);
	uvzShadow += normalShadow * g_normalOffsetShadow;

	// Do the samples - each one a 2x2 GatherCmp
	// !!!UNDONE: offset z comparison values to handle sloped receiver
	float4 samplesNW = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2(-1, -1));
	float4 samplesNE = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2( 1, -1));
	float4 samplesSW = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2(-1,  1));
	float4 samplesSE = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2( 1,  1));

	// Calculate fractional location relative to texel centers.  The 1/512 offset is needed to ensure
	// that frac()'s output steps from 1 to 0 at the exact same point that GatherCmp switches texels.
	float2 offset = frac(uvzShadow.xy * g_dimsShadowMap + (-0.5 + 1.0/512.0));

	// Calculate weights for the samples based on a 2px-radius biquadratic filter
	static const float radius = 2.0;
	float4 xOffsets = offset.x + float4(1, 0, -1, -2);
	float4 yOffsets = offset.y + float4(1, 0, -1, -2);
	// Readable version: xWeights = max(0, 1 - x^2/r^2) for x in xOffsets
	float4 xWeights = saturate(square(xOffsets) * (-1.0 / square(radius)) + 1.0);
	float4 yWeights = saturate(square(yOffsets) * (-1.0 / square(radius)) + 1.0);

	// Calculate weighted sum of samples
	float sampleSum = dot(xWeights.xyyx, yWeights.yyxx * samplesNW) +
					  dot(xWeights.zwwz, yWeights.yyxx * samplesNE) +
					  dot(xWeights.xyyx, yWeights.wwzz * samplesSW) +
					  dot(xWeights.zwwz, yWeights.wwzz * samplesSE);
	float weightSum = dot(xWeights.xyyx, yWeights.yyxx) +
					  dot(xWeights.zwwz, yWeights.yyxx) +
					  dot(xWeights.xyyx, yWeights.wwzz) +
					  dot(xWeights.zwwz, yWeights.wwzz);

	return sharpen(saturate(sampleSum / weightSum), g_shadowSharpening);
}

float EvaluateShadow(
	float4 uvzwShadow,
	float3 normalGeom)
{
	return EvaluateShadowGather16(uvzwShadow, normalGeom);
}

#endif // !defined(SHADER_COMMON_H)
