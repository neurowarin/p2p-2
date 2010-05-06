#include "init.hpp"
#include "addr_impl.hpp"
#include <net/net.hpp>

net::listener::listener()
{

}

net::listener::listener(const endpoint & E)
{
	open(E);
}

boost::shared_ptr<net::nstream> net::listener::accept()
{
	assert(is_open());
	addrinfo ai;
	sockaddr_storage sas;
	ai.ai_addr = reinterpret_cast<sockaddr *>(&sas);
	ai.ai_addrlen = sizeof(sockaddr_storage);
	int new_socket = ::accept(socket_FD, ai.ai_addr,
		reinterpret_cast<socklen_t *>(&ai.ai_addrlen));
	if(new_socket == -1){
		return boost::shared_ptr<nstream>();
	}
	boost::shared_ptr<nstream> N(new nstream(new_socket));
	return N;
}

void net::listener::open(const endpoint & E)
{
	close();
	if((socket_FD = ::socket(E.AI->ai.ai_family, SOCK_STREAM,
		E.AI->ai.ai_protocol)) == -1)
	{
		LOG << strerror(errno);
		close();
		return;
	}
	/*
	This takes care of "address already in use" errors that can happen when
	trying to bind to a port that wasn't properly closed and hasn't timed
	out.
	*/
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	if(::setsockopt(socket_FD, SOL_SOCKET, SO_REUSEADDR,
		reinterpret_cast<char *>(&optval), optlen) == -1)
	{
		LOG << strerror(errno);
		close();
		return;
	}
	if(::bind(socket_FD, E.AI->ai.ai_addr, E.AI->ai.ai_addrlen) == -1){
		LOG << strerror(errno);
		close();
		return;
	}
	int backlog = 512; //max pending accepts
	if(::listen(socket_FD, backlog) == -1){
		LOG << strerror(errno);
		close();
		return;
	}
}

std::string net::listener::port()
{
	if(socket_FD == -1){
		return "";
	}
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(getsockname(socket_FD, reinterpret_cast<sockaddr *>(&addr), &addrlen) == -1){
		LOG << strerror(errno);
		close();
		return "";
	}
	char buf[6];
	if(getnameinfo(reinterpret_cast<sockaddr *>(&addr), addrlen, NULL, 0, buf,
		sizeof(buf), NI_NUMERICSERV) == -1)
	{
		LOG << strerror(errno);
		close();
		return "";
	}
	return buf;
}
