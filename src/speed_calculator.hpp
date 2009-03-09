//THREADSAFE
#ifndef H_SPEED_CALCULATOR
#define H_SPEED_CALCULATOR

//boost
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

//custom
#include "settings.hpp"

//std
#include <cmath>
#include <ctime>
#include <deque>

class speed_calculator
{
public:
	speed_calculator();

	/*
	current_second_bytes:
		Returns how many bytes have been received in the current second.
	speed:
		Returns average speed.
	update:
		Add n_bytes to the average.
	*/
	unsigned current_second_bytes();
	unsigned speed();
	void update(const unsigned & n_bytes);

private:
	//mutex for all public functions
	boost::shared_ptr<boost::recursive_mutex> Recursive_Mutex;

	//average speed over settings::SPEED_AVERAGE seconds
	unsigned average;

	/*
	pair<second, bytes in second>
	The low elements are more current in time.
	*/
	std::pair<time_t, unsigned> Second_Bytes[settings::SPEED_AVERAGE + 1];
};
#endif
