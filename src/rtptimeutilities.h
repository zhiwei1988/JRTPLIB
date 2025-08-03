/**
 * \file rtptimeutilities.h
 */

#ifndef RTPTIMEUTILITIES_H

#define RTPTIMEUTILITIES_H

#include "rtpconfig.h"
#include <cstdint>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define RTP_NTPTIMEOFFSET									2208988800UL

#define C1000000 1000000ULL

/**
 * This is a simple wrapper for the most significant word (MSW) and least 
 * significant word (LSW) of an NTP timestamp.
 */
class RTPNTPTime
{
public:
	/** This constructor creates and instance with MSW \c m and LSW \c l. */
	RTPNTPTime(uint32_t m,uint32_t l)					{ msw = m ; lsw = l; }

	/** Returns the most significant word. */
	uint32_t GetMSW() const								{ return msw; }

	/** Returns the least significant word. */
	uint32_t GetLSW() const								{ return lsw; }
private:
	uint32_t msw,lsw;
};

/** This class is used to specify wallclock time, delay intervals etc.
 *  This class is used to specify wallclock time, delay intervals etc. 
 *  It stores a number of seconds and a number of microseconds.
 */
class RTPTime
{
public:
	/** Returns an RTPTime instance representing the current wallclock time. 
	 *  Returns an RTPTime instance representing the current wallclock time. This is expressed 
	 *  as a number of seconds since 00:00:00 UTC, January 1, 1970.
	 */
	static RTPTime CurrentTime();

	/** This function waits the amount of time specified in \c delay. */
	static void Wait(const RTPTime &delay);
		
	/** Creates an RTPTime instance representing \c t, which is expressed in units of seconds. */
	RTPTime(double t);

	/** Creates an instance that corresponds to \c ntptime. 
	 *  Creates an instance that corresponds to \c ntptime.  If
	 *  the conversion cannot be made, both the seconds and the
	 *  microseconds are set to zero.
	 */
	RTPTime(RTPNTPTime ntptime);

	/** Creates an instance corresponding to \c seconds and \c microseconds. */
	RTPTime(int64_t seconds, uint32_t microseconds);

	/** Returns the number of seconds stored in this instance. */
	int64_t GetSeconds() const;

	/** Returns the number of microseconds stored in this instance. */
	uint32_t GetMicroSeconds() const;

	/** Returns the time stored in this instance, expressed in units of seconds. */
	double GetDouble() const 										{ return m_t; }

	/** Returns the NTP time corresponding to the time stored in this instance. */
	RTPNTPTime GetNTPTime() const;

	RTPTime &operator-=(const RTPTime &t);
	RTPTime &operator+=(const RTPTime &t);
	bool operator<(const RTPTime &t) const;
	bool operator>(const RTPTime &t) const;
	bool operator<=(const RTPTime &t) const;
	bool operator>=(const RTPTime &t) const;

	bool IsZero() const { return m_t == 0; }
private:
	double m_t;
};

inline RTPTime::RTPTime(double t)
{
	m_t = t;
}

inline RTPTime::RTPTime(int64_t seconds, uint32_t microseconds)
{
	if (seconds >= 0)
	{
		m_t = (double)seconds + 1e-6*(double)microseconds;
	}
	else
	{
		int64_t possec = -seconds;

		m_t = (double)possec + 1e-6*(double)microseconds;
		m_t = -m_t;
	}
}

inline RTPTime::RTPTime(RTPNTPTime ntptime)
{
	if (ntptime.GetMSW() < RTP_NTPTIMEOFFSET)
	{
		m_t = 0;
	}
	else
	{
		uint32_t sec = ntptime.GetMSW() - RTP_NTPTIMEOFFSET;
		
		double x = (double)ntptime.GetLSW();
		x /= (65536.0*65536.0);
		x *= 1000000.0;
		uint32_t microsec = (uint32_t)x;

		m_t = (double)sec + 1e-6*(double)microsec;
	}
}

inline int64_t RTPTime::GetSeconds() const
{
	return (int64_t)m_t;
}

inline uint32_t RTPTime::GetMicroSeconds() const
{
	uint32_t microsec;

	if (m_t >= 0)
	{
		int64_t sec = (int64_t)m_t;
		microsec = (uint32_t)(1e6*(m_t - (double)sec) + 0.5);
	}
	else // m_t < 0
	{
		int64_t sec = (int64_t)(-m_t);
		microsec = (uint32_t)(1e6*((-m_t) - (double)sec) + 0.5);
	}

	if (microsec >= 1000000)
		return 999999;
	// Unsigned, it can never be less than 0
	// if (microsec < 0)
	// 	return 0;
	return microsec;
}

// Linux time implementation

#ifdef RTP_HAVE_CLOCK_GETTIME
inline double RTPTime_timespecToDouble(struct timespec &ts)
{
	return (double)ts.tv_sec + 1e-9*(double)ts.tv_nsec;
}

inline RTPTime RTPTime::CurrentTime()
{
	static bool s_initialized = false;
	static double s_startOffet = 0;

	if (!s_initialized)
	{
		s_initialized = true;

		// Get the corresponding times in system time and monotonic time
		struct timespec tpSys, tpMono;

		clock_gettime(CLOCK_REALTIME, &tpSys);
		clock_gettime(CLOCK_MONOTONIC, &tpMono);

		double tSys = RTPTime_timespecToDouble(tpSys);
		double tMono = RTPTime_timespecToDouble(tpMono);

		s_startOffet = tSys - tMono;
		return tSys;
	}

	struct timespec tpMono;
	clock_gettime(CLOCK_MONOTONIC, &tpMono);

	double tMono0 = RTPTime_timespecToDouble(tpMono);
	return tMono0 + s_startOffet;
}

#else // gettimeofday fallback

inline RTPTime RTPTime::CurrentTime()
{
	struct timeval tv;
	
	gettimeofday(&tv,0);
	return RTPTime((uint64_t)tv.tv_sec,(uint32_t)tv.tv_usec);
}
#endif // RTP_HAVE_CLOCK_GETTIME

inline void RTPTime::Wait(const RTPTime &delay)
{
	if (delay.m_t <= 0)
		return;

	uint64_t sec = (uint64_t)delay.m_t;
	uint64_t nanosec = (uint32_t)(1e9*(delay.m_t-(double)sec));

	struct timespec req,rem;
	int ret;

	req.tv_sec = (time_t)sec;
	req.tv_nsec = ((long)nanosec);
	do
	{
		ret = nanosleep(&req,&rem);
		req = rem;
	} while (ret == -1 && errno == EINTR);
}

inline RTPTime &RTPTime::operator-=(const RTPTime &t)
{ 
	m_t -= t.m_t;
	return *this;
}

inline RTPTime &RTPTime::operator+=(const RTPTime &t)
{ 
	m_t += t.m_t;
	return *this;
}

inline RTPNTPTime RTPTime::GetNTPTime() const
{
	uint32_t sec = (uint32_t)m_t;
	uint32_t microsec = (uint32_t)((m_t - (double)sec)*1e6);

	uint32_t msw = sec+RTP_NTPTIMEOFFSET;
	uint32_t lsw;
	double x;
	
    x = microsec/1000000.0;
	x *= (65536.0*65536.0);
	lsw = (uint32_t)x;

	return RTPNTPTime(msw,lsw);
}

inline bool RTPTime::operator<(const RTPTime &t) const
{
	return m_t < t.m_t;
}

inline bool RTPTime::operator>(const RTPTime &t) const
{
	return m_t > t.m_t;
}

inline bool RTPTime::operator<=(const RTPTime &t) const
{
	return m_t <= t.m_t;
}

inline bool RTPTime::operator>=(const RTPTime &t) const
{
	return m_t >= t.m_t;
}

class RTPTimeInitializerObject
{
public:
	RTPTimeInitializerObject();
	void Dummy() { dummy++; }
private:
	int dummy;
};

extern RTPTimeInitializerObject timeinit;

#endif // RTPTIMEUTILITIES_H

