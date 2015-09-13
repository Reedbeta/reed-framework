#ifndef SHADER_SLOTS_H
#define SHADER_SLOTS_H

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

#define TEX_DIFFUSE						TEXREG(0)
#define TEX_SHADOW						TEXREG(1)

#define SAMP_DEFAULT					SAMPREG(0)
#define SAMP_SHADOW						SAMPREG(1)

#endif // !defined(SHADER_SLOTS_H)
