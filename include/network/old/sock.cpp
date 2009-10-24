#include <network/network.hpp>

network::sock::sock(const int socket_FD_in):
	info(new wrapper::address_info()),
	socket_FD(socket_FD_in),
	IP(wrapper::get_IP(socket_FD)),
	port(wrapper::get_port(socket_FD)),
	Direction(incoming),
	connect_flag(false),
	disconnect_flag(false),
	failed_connect_flag(false),
	recv_flag(false),
	send_flag(false),
	latest_recv(0),
	latest_send(0),
	Error(no_error),
	idle_timeout(16),
	last_active(std::time(NULL))
{

}

network::sock::sock(boost::shared_ptr<wrapper::address_info> info_in):
	info(info_in),
	socket_FD(-1),
	host(info->get_host()),
	IP(wrapper::get_IP(*info)),
	port(info->get_port()),
	Direction(outgoing),
	connect_flag(false),
	disconnect_flag(false),
	failed_connect_flag(false),
	recv_flag(false),
	send_flag(false),
	latest_recv(0),
	latest_send(0),
	Error(no_error),
	idle_timeout(16),
	last_active(std::time(NULL))
{

}

network::sock::~sock()
{
	if(socket_FD != -1){
		close(socket_FD);
	}
}

void network::sock::seen()
{
	last_active = std::time(NULL);
}

//returns true if the socket has timed out
bool network::sock::timeout()
{
	return std::time(NULL) - last_active > idle_timeout;
}
