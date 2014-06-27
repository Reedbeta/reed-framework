#include "framework.h"

namespace Framework
{
	// GPUProfiler implementation

	GPUProfiler::GPUProfiler()
	:	m_msAvg(),
		m_disjointQueries(),
		m_timestampQueries(),
		m_msSum(),
		m_markerCount(0),
		m_framesToBuffer(0),
		m_framesToAverage(0),
		m_iFrameCur(0),
		m_framesSummed(0),
		m_framesIssued(0)
	{
	}

	void GPUProfiler::Init(
		ID3D11Device * pDevice,
		int markerCount,
		int framesToBuffer /*= 3*/,
		int framesToAverage /*= 30*/)
	{
		ASSERT_ERR(pDevice);
		ASSERT_ERR(markerCount >= 0);
		ASSERT_ERR(framesToBuffer >= 1);
		ASSERT_ERR(framesToAverage >= 1);

		m_markerCount = markerCount;
		m_framesToBuffer = framesToBuffer;
		m_framesToAverage = framesToAverage;

		// Resize all the vectors appropriately
		m_msAvg.clear();			m_msAvg.resize(markerCount + 1);
		m_disjointQueries.clear();	m_disjointQueries.resize(framesToBuffer);
		m_timestampQueries.clear();	m_timestampQueries.resize(framesToBuffer * (markerCount + 2));
		m_msSum.clear();			m_msSum.resize(markerCount + 1);

		m_iFrameCur = 0;
		m_framesSummed = 0;
		m_framesIssued = 0;

		// Create all queries
		D3D11_QUERY_DESC queryDesc = { D3D11_QUERY_TIMESTAMP_DISJOINT };
		for (int i = 0; i < framesToBuffer; ++i)
		{
			CHECK_D3D(pDevice->CreateQuery(&queryDesc, &m_disjointQueries[i]));
		}
		queryDesc.Query = D3D11_QUERY_TIMESTAMP;
		for (int i = 0, c = int(m_timestampQueries.size()); i < c; ++i)
		{
			CHECK_D3D(pDevice->CreateQuery(&queryDesc, &m_timestampQueries[i]));
		}
	}

	void GPUProfiler::OnFrameStart(ID3D11DeviceContext * pCtx)
	{
		ASSERT_ERR(pCtx);

		pCtx->Begin(m_disjointQueries[m_iFrameCur]);
		pCtx->End(m_timestampQueries[m_iFrameCur * (m_markerCount + 2)]);
	}

	void GPUProfiler::Mark(ID3D11DeviceContext * pCtx, int iMarker)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(iMarker >= 0 && iMarker < m_markerCount);

		pCtx->End(m_timestampQueries[m_iFrameCur * (m_markerCount + 2) + iMarker + 1]);
	}

	template <typename T>
	inline void WaitForQuery(ID3D11DeviceContext * pCtx, ID3D11Query * pQuery, T * pData)
	{
		HRESULT res = pCtx->GetData(pQuery, pData, sizeof(T), 0);
		ASSERT_WARN(SUCCEEDED(res));

		while (res == S_FALSE)
		{
			Sleep(1);
			res = pCtx->GetData(pQuery, pData, sizeof(T), 0);
			ASSERT_WARN(SUCCEEDED(res));
		}
	}

	void GPUProfiler::OnFrameEnd(ID3D11DeviceContext * pCtx)
	{
		ASSERT_ERR(pCtx);

		pCtx->End(m_timestampQueries[m_iFrameCur * (m_markerCount + 2) + m_markerCount + 1]);
		pCtx->End(m_disjointQueries[m_iFrameCur]);

		// Switch to the next set of queries for next frame
		++m_framesIssued;
		m_iFrameCur = (m_iFrameCur + 1) % m_framesToBuffer;

		if (m_framesIssued < m_framesToBuffer)
		{
			// We're still starting up and haven't issued enough frames to gather data yet
			return;
		}

		// Gather the oldest set of query data
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
		std::vector<u64> timestamps(m_markerCount + 2);
		WaitForQuery(pCtx, m_disjointQueries[m_iFrameCur], &disjointData);
		for (int i = 0; i < m_markerCount + 2; ++i)
		{
			WaitForQuery(
				pCtx,
				m_timestampQueries[m_iFrameCur * (m_markerCount + 2) + i],
				&timestamps[i]);
		}

		// If it's disjoint, gotta throw it out
		if (disjointData.Disjoint)
			return;

		// Convert to milliseconds and accumulate onto sum
		for (int i = 0; i < m_markerCount; ++i)
		{
			m_msSum[i] += 1000.0f * float(timestamps[i+1] - timestamps[i]) / float(disjointData.Frequency);
		}

		// The last element is the whole-frame time
		m_msSum[m_markerCount] += 1000.0f * float(timestamps[m_markerCount+1] - timestamps[0]) / float(disjointData.Frequency);

		// Recalculate averages if necessary
		++m_framesSummed;
		if (m_framesSummed >= m_framesToAverage)
		{
			for (int i = 0; i < m_markerCount + 1; ++i)
			{
				m_msAvg[i] = m_msSum[i] / m_framesSummed;
				m_msSum[i] = 0.0f;
			}

			m_framesSummed = 0;
		}
	}
}
