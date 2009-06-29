#ifndef H_PORTABLE_SLEEP
#define H_PORTABLE_SLEEP

//standard
#include <cassert>

#ifdef _WIN32
	#include <windows.h>
#endif

//replacement for system specific sleep functions
namespace portable_sleep
{
//sleep for specified number of milliseconds
inline void ms(const unsigned & ms)
{
	//if milliseconds = 0 then yield should be used
	assert(ms != 0);
	#ifdef _WIN32
	Sleep(ms);
	#else
	usleep(ms * 1000);
	#endif
}

//yield time slice
inline void yield()
{
	#ifdef _WIN32
	Sleep(0);
	#else
	usleep(1);
	#endif
}
} //end of namespace portable_sleep
#endif
