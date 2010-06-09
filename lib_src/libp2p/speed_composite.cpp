#include "speed_composite.hpp"

void speed_composite::add(const unsigned n_bytes)
{
	for(std::list<boost::shared_ptr<net::speed_calc> >::iterator
		it_cur = Elements.begin(), it_end = Elements.end(); it_cur != it_end; ++it_cur)
	{
		(*it_cur)->add(n_bytes);
	}
}

void speed_composite::add_calc(boost::shared_ptr<net::speed_calc> SC)
{
	if(SC){
		Elements.push_back(SC);
	}
}
