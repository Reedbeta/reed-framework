#include "framework.h"

namespace Framework
{
	// Timer implementation

	Timer::Timer()
	:	m_timestep(0.0f),
		m_time(0.0f),
		m_frameCount(0),
		m_iFrameCur(0)
	{
		i64 frequency;
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
		m_period = 1.0f / float(frequency);

		QueryPerformanceCounter((LARGE_INTEGER *)&m_startupTimestamp);
		for (int i = 0; i < dim(m_lastFrameTimestamps); ++i)
			m_lastFrameTimestamps[i] = m_startupTimestamp;
	}

	void Timer::OnFrameStart()
	{
		++m_frameCount;

		i64 timestamp;
		QueryPerformanceCounter((LARGE_INTEGER *)&timestamp);

		m_time = float(timestamp - m_startupTimestamp) * m_period;

		// Calculate smoothed timestep over several frames,
		// to help with microstuttering.  Maintain a ring buffer
		// of frame timestamps to implement this.
		m_timestep = float(timestamp - m_lastFrameTimestamps[m_iFrameCur]) * 
						m_period / float(dim(m_lastFrameTimestamps));
		m_lastFrameTimestamps[m_iFrameCur] = timestamp;
		m_iFrameCur = (m_iFrameCur + 1) % dim(m_lastFrameTimestamps);
	}
}
