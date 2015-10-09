#pragma once

namespace Framework
{
	class Timer
	{
	public:
				Timer();
		void	OnFrameStart();

		float	m_timestep;					// Delta time in seconds between frames, averaged over last few frames
		float	m_time;						// Time in seconds since startup
		int		m_frameCount;				// Frames since startup

		i64		m_startupTimestamp;			// QPC time of startup
		i64		m_lastFrameTimestamps[3];	// Ring buffer of QPC times of last few frames
		int		m_iFrameCur;				// Write index into ring buffer
		float	m_period;					// QPC period in seconds
	};
}
