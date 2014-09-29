#pragma once

namespace Framework
{
	class Timer
	{
	public:
				Timer();
		void	OnFrameStart();

		float	m_timestep;
		float	m_time;
		int		m_frameCount;

		i64		m_startupTimestamp;
		i64		m_lastFrameTimestamps[3];
		int		m_iFrameCur;
		float	m_period;
	};
}
