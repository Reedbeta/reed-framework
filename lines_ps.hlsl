struct LineVertex	// Matches struct LineVertex in d3d11-window.h and lines_vs.hlsl
{
	float4	m_rgba : COLOR;
	float4	m_posClip : SV_Position;
};

void main(
	in LineVertex i_vtx,
	out float4 o_rgba : SV_Target0)
{
	o_rgba = i_vtx.m_rgba;
}
