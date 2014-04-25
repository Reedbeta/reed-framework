#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

// This file is included from both C++ and HLSL; it defines shared resource slot assignments

#ifdef __cplusplus
#	define CBREG(n)						n
#	define TEXREG(n)					n
#	define SAMPREG(n)					n
#else
#	define CBREG(n)						register(b##n)
#	define TEXREG(n)					register(t##n)
#	define SAMPREG(n)					register(s##n)
#endif

#define CB_FRAME						CBREG(0)
#define CB_SHADER						CBREG(1)
#define CB_DEBUG						CBREG(2)



#ifndef __cplusplus

#pragma pack_matrix(row_major)

struct Vertex
{
	float3		m_pos		: POSITION;
	float3		m_normal	: NORMAL;
	float2		m_uv		: UV;
};

cbuffer CBFrame : CB_FRAME					// matches struct CBFrame in mcwindow.h
{
	float4x4	g_matWorldToClip;
	float4x4	g_matWorldToUvzwShadow;
	float3		g_posCamera;

	float3		g_vecDirectionalLight;
	float3		g_rgbDirectionalLight;

	float		g_exposure;					// Exposure multiplier
}

cbuffer CBDebug : CB_DEBUG			// matches struct CBDebug in mcwindow.h
{
	float		g_debugKey;			// Mapped to spacebar - 0 if up, 1 if down
	float		g_debugSlider0;		// Mapped to debug sliders in UI
	float		g_debugSlider1;		// ...
	float		g_debugSlider2;		// ...
	float		g_debugSlider3;		// ...
}

float square(float x) { return x*x; }
float2 square(float2 x) { return x*x; }
float3 square(float3 x) { return x*x; }
float4 square(float4 x) { return x*x; }

#endif // !defined(__cplusplus)
#endif // !defined(SHADER_COMMON_H)
