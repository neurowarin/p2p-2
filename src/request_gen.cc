#include "request_gen.h"

request_gen::request_gen(
):
	latest_request(0),
	initialized(false)
{

}

void request_gen::check_timeouts()
{
	//do re_request on requests that are older than max_age.
	std::map<boost::uint64_t, time_t>::iterator iter_cur, iter_end;
	iter_cur = unfulfilled_request.begin();
	iter_end = unfulfilled_request.end();
	while(iter_cur != iter_end){
		if(iter_cur->second <= time(NULL)){
			re_request.insert(iter_cur->first);
		}
		++iter_cur;
	}
}

bool request_gen::complete()
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(initialized);
	return (latest_request == max_request && unfulfilled_request.empty() && re_request.empty() && re_requested.empty());
}

void request_gen::force_re_request(const boost::uint64_t & number)
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(initialized);
	re_request.insert(number);
}

void request_gen::fulfil(const boost::uint64_t & fulfilled_request)
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(initialized);
	unfulfilled_request.erase(fulfilled_request);
	re_request.erase(fulfilled_request);
	re_requested.erase(fulfilled_request);
}

boost::uint64_t request_gen::highest_requested()
{
	boost::mutex::scoped_lock lock(Mutex);
	return latest_request;
}

void request_gen::init(const boost::uint64_t & min_request_in, const boost::uint64_t & max_request_in, const int & timeout_in)
{
	initialized = true;
	unfulfilled_request.clear();
	re_request.clear();
	re_requested.clear();
	latest_request = min_request_in;
	min_request = min_request_in;
	max_request = max_request_in;
	timeout = timeout_in;
}

bool request_gen::request(std::deque<boost::uint64_t> & prev_request)
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(initialized);
	check_timeouts();
	if(re_request.empty()){
		//no re_requests to be made
		if(latest_request == max_request){
			/*
			On the last request. Check to see if it has already been requested from
			a server. If it has been return false because no more requests need to
			be made.
			*/
			std::map<boost::uint64_t, time_t>::iterator iter_cur, iter_end;
			iter_cur = unfulfilled_request.begin();
			iter_end = unfulfilled_request.end();
			while(iter_cur != iter_end){
				if(iter_cur->first == max_request){
					return false;
				}
				++iter_cur;
			}
			prev_request.push_back(max_request);
			unfulfilled_request.insert(std::make_pair(max_request, time(NULL) + timeout * prev_request.size()));
			return true;
		}else{
			//not on last request, proceed normally adding the next sequential request
			prev_request.push_back(latest_request);
			unfulfilled_request.insert(std::make_pair(latest_request, time(NULL) + timeout * prev_request.size()));
			++latest_request;
			return true;
		}
	}else{
		/*
		A re_request needs to be done. Only do re_requests from servers that haven't
		already had a block re_requested from them.
		*/
		std::set<boost::uint64_t>::iterator re_requested_iter_cur, re_requested_iter_end;
		std::deque<boost::uint64_t>::iterator prev_iter_cur, prev_iter_end;
		re_requested_iter_cur = re_requested.begin();
		re_requested_iter_end = re_requested.end();
		while(re_requested_iter_cur != re_requested_iter_end){
			prev_iter_cur = prev_request.begin();
			prev_iter_end = prev_request.end();
			while(prev_iter_cur != prev_iter_end){
				if(*re_requested_iter_cur == *prev_iter_cur){
					/*
					A re-request was already made from this server. It won't be allowed
					to handle another re-request until it handles the one it already has.
					*/
					return false;
				}
				++prev_iter_cur;
			}
			++re_requested_iter_cur;
		}
		prev_request.push_back(*(re_request.begin()));
		unfulfilled_request.insert(std::make_pair(*(re_request.begin()), time(NULL) + timeout * prev_request.size()));
		re_requested.insert(*(re_request.begin()));
		re_request.erase(re_request.begin());
		logger::debug(LOGGER_P1,"re_requesting ",*(prev_request.begin()));
		return true;
	}
}
