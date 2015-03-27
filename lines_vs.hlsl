struct LineVertex	// Matches struct LineVertex in d3d11-window.h and lines_ps.hlsl
{
	float4	m_rgba : COLOR;
	float4	m_posClip : SV_Position;
};

void main(
	in LineVertex i_vtx,
	out LineVertex o_vtx)
{
	o_vtx = i_vtx;
}
