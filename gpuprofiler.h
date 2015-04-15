#pragma once

namespace Framework
{
	// Generic profiler that manages query objects, buffers a few frames,
	// and averages timings over a short period for viewability.

	class GPUProfiler
	{
	public:
				GPUProfiler();
		void	Init(
					ID3D11Device * pDevice,
					int markerCount,
					int framesToBuffer = 3,
					int framesToAverage = 30);
		void	Reset();

		void	OnFrameStart(ID3D11DeviceContext * pCtx);
		void	Mark(ID3D11DeviceContext * pCtx, int iMarker);
		void	OnFrameEnd(ID3D11DeviceContext * pCtx);

		std::vector<float>	m_msAvg;		// Average milliseconds since previous marker, for each marker + 1 (whole frame time)
		
		std::vector<comptr<ID3D11Query>>	m_disjointQueries;			// Disjoint query per buffered frame
		std::vector<comptr<ID3D11Query>>	m_timestampQueries;			// Timestamp queries, for each marker + 2 (start/end of frame) per buffered frame
		std::vector<float>					m_msSum;					// Summed timings waiting for average
		int									m_markerCount;
		int									m_framesToBuffer;
		int									m_framesToAverage;
		int									m_iFrameCur;				// Which buffered frame are we on
		int									m_framesSummed;
		int									m_framesIssued;
	};
}
