#include "exchange_udp.hpp"

//BEGIN expect_response_element
exchange_udp::expect_response_element::expect_response_element(
	boost::shared_ptr<message_udp::recv::base> message_in,
	boost::function<void()> timeout_call_back_in
):
	message(message_in),
	timeout_call_back(timeout_call_back_in),
	time_first_expected(std::time(NULL))
{

}

bool exchange_udp::expect_response_element::timed_out()
{
	return std::time(NULL) - time_first_expected > 60;
}
//END expect_response_element

exchange_udp::exchange_udp()
{
	//setup UDP listener
	std::set<network::endpoint> E = network::get_endpoint(
		"",
		database::table::prefs::get_port(),
		network::udp
	);
	assert(!E.empty());
	ndgram.open(*E.begin());
	assert(ndgram.is_open());
}

void exchange_udp::check_timeouts()
{
	for(std::multimap<network::endpoint, expect_response_element>::iterator
		iter_cur = Expect_Response.begin(), iter_end = Expect_Response.end();
		iter_cur != iter_end;)
	{
		if(iter_cur->second.timed_out()){
LOGGER;
			if(iter_cur->second.timeout_call_back){
LOGGER;
				iter_cur->second.timeout_call_back();
			}
			Expect_Response.erase(iter_cur++);
		}else{
			++iter_cur;
		}
	}
}

void exchange_udp::expect_anytime(boost::shared_ptr<message_udp::recv::base> M)
{
	Expect_Anytime.push_back(M);
}

void exchange_udp::expect_anytime_remove(boost::shared_ptr<message_udp::send::base> M)
{
	for(std::list<boost::shared_ptr<message_udp::recv::base> >::iterator
		iter_cur = Expect_Anytime.begin(), iter_end = Expect_Anytime.end();
		iter_cur != iter_end;)
	{
		if((*iter_cur)->expect(M->buf)){
			iter_cur = Expect_Anytime.erase(iter_cur);
		}else{
			++iter_cur;
		}
	}
}

void exchange_udp::expect_response(boost::shared_ptr<message_udp::recv::base> M,
	const network::endpoint & endpoint,
	boost::function<void()> timeout_call_back)
{
	Expect_Response.insert(std::make_pair(endpoint,
		expect_response_element(M, timeout_call_back)));
}

void exchange_udp::tick()
{
	//wait for message to arrive
	std::set<int> read, write;
	read.insert(ndgram.socket());
	select(read, write, 1000);

	if(read.empty()){
		//nothing received
		check_timeouts();
		return;
	}

	//recv message
	boost::shared_ptr<network::endpoint> from;
	network::buffer recv_buf;
	ndgram.recv(recv_buf, from);
	assert(from);

	//check if expected response
	std::pair<std::multimap<network::endpoint, expect_response_element>::iterator,
		std::multimap<network::endpoint, expect_response_element>::iterator >
		range = Expect_Response.equal_range(*from);
	for(; range.first != range.second; ++range.first){
		if(range.first->second.message->recv(recv_buf, *from)){
			Expect_Response.erase(range.first);
			return;
		}

	}

	//check if expected anytime
	for(std::list<boost::shared_ptr<message_udp::recv::base> >::iterator
		iter_cur = Expect_Anytime.begin(), iter_end = Expect_Anytime.end();
		iter_cur != iter_end; ++iter_cur)
	{
		if((*iter_cur)->recv(recv_buf, *from)){
			return;
		}
	}

	check_timeouts();
}

void exchange_udp::send(boost::shared_ptr<message_udp::send::base> M,
	const network::endpoint & endpoint)
{
	ndgram.send(M->buf, endpoint);
}
