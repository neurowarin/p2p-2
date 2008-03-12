//std
#include <cmath>
#include <deque>

#include "speed_calculator.h"

speed_calculator::speed_calculator()
{
	average_speed = 0;
	rate_control_counter = 0;
	rate_control_damper = 1;
}

unsigned int speed_calculator::speed()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(AS_mutex);
	return average_speed;
	}//end lock scope
}

void speed_calculator::update(const unsigned int & byte_count)
{
	/*
	This function keeps track of how many bytes are downloaded in one second. It
	has a vector that keeps track of bytes in the current second and bytes in the
	previous second. Because the current second will always be incomplete, the
	previous second will be considered the download speed.
	*/
	unsigned int current_time = time(0);

	if(Second_Bytes.empty()){
		Second_Bytes.push_front(std::make_pair(current_time, 0));
	}

	if(Second_Bytes.front().first != current_time){
		//on a new second
		Second_Bytes.push_front(std::make_pair(current_time, byte_count));
	}
	else{
		Second_Bytes.front().second += byte_count;
	}

	//the current second is never counted in the average because it's incomplete
	if(Second_Bytes.size() > global::SPEED_AVERAGE + 1){
		Second_Bytes.pop_back();
	}

	unsigned int total_bytes = 0;
	std::deque<std::pair<unsigned int, unsigned int> >::reverse_iterator iter_cur, iter_end;
	iter_cur = Second_Bytes.rbegin();
	iter_end = Second_Bytes.rend();
	int count = 0;
	while(iter_cur != iter_end && iter_cur->first != current_time){
		total_bytes += iter_cur->second;
		++count;
		++iter_cur;
	}

	{//begin lock scope
	boost::mutex::scoped_lock lock(AS_mutex);
	if(count != 0){
		average_speed = total_bytes / count;
	}
	else{
		average_speed = 0;
	}
	}//end lock scope
}

unsigned int speed_calculator::rate_control(const int & rate, int max_possible_transfer)
{
	++rate_control_counter;
	unsigned int transfer = 0;

	if(average_speed == 0){
		rate_control_damper = 1;
	}

	if(!Second_Bytes.empty()){
		if(Second_Bytes.front().second < rate){
			transfer = rate - Second_Bytes.front().second;
		}
	}

	if(transfer > max_possible_transfer){
		transfer = max_possible_transfer;
	}

	//save CPU by sleeping so long as it doesn't hurt transfer speed
	if(transfer == 0){
		if(rate_control_counter % rate_control_damper == 0){
			usleep(1);

			if(1 - (float)(rate - average_speed)/(float)(rate + average_speed) < 0.95){
				++rate_control_damper;
			}
			else{
				--rate_control_damper;
				if(rate_control_damper < 1){
					rate_control_damper = 1;
				}
			}
		}
	}

	return transfer;
}
