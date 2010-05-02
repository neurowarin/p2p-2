#ifndef H_NETWORK_ADDR_IMPL
#define H_NETWORK_ADDR_IMPL

//custom
#include "system_include.hpp"

//standard
#include <cassert>
#include <cstring>

namespace network{
class addr_impl
{
public:
	addr_impl(const addrinfo * ai_in);
	addr_impl(const addr_impl & AI);

	addrinfo ai;
	sockaddr_storage sas;

	addr_impl & operator = (const addr_impl & AI);

private:
	void copy(const addrinfo * ai_in);
};
}//end namespace network
#endif
