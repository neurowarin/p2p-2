#ifndef H_NETWORK_SOCKET_DATA
#define H_NETWORK_SOCKET_DATA

//boost
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

//custom
#include "wrapper.hpp"

//include
#include <buffer.hpp>

//std
#include <string>

namespace network{
class socket_data
{
public:
	socket_data(
		const int socket_FD_in,
		const std::string & host_in,
		const std::string & IP_in,
		const std::string & port_in,
		boost::shared_ptr<wrapper::info> Info_in
			= boost::shared_ptr<wrapper::info>(new wrapper::info(NULL, NULL))
	):
		socket_FD(socket_FD_in),
		host(host_in),
		IP(IP_in),
		port(port_in),
		Info(Info_in),
		failed_connect_flag(false),
		connect_flag(false),
		recv_flag(false),
		send_flag(false),
		disconnect_flag(false),
		Error(NONE),
		last_seen(std::time(NULL)),
		Socket_Data_Visible(
			socket_FD,
			host,
			IP,
			port,
			disconnect_flag,
			recv_buff,
			send_buff,
			Error
		)
	{}

	const int socket_FD;    //should not be used except by reactor_*
	const std::string host; //name we connected to (ie "google.com")
	const std::string IP;   //IP host resolved to
	const std::string port; //if listen_port == port then connection is incoming
	std::time_t last_seen;  //last time seen

	/*
	failed_connect_flag:
		If true the failed connect call back needs to be done. No other call
		backs should be done after the failed connect call back.
	connect_flag:
		If false the connect call back needs to be done. After the call back is
		done this is set to true.
	recv_flag:
		If true the recv call back needs to be done.
	send_flag:
		If true the send call back needs to be done.
	disconnect_flag:
		If this is true after get_job returns the reactor disconnected the
		socket (because the other end hung up). If this is true when the
		socket_data is passed to finish_job the socket will be disconnected if
		it wasn't already.			
	*/
	bool failed_connect_flag;
	bool connect_flag;
	bool recv_flag;
	bool send_flag;
	bool disconnect_flag;

	//send()/recv() buffers
	buffer recv_buff;
	buffer send_buff;

	/*
	This error condition is set when a connection fails.
	*/
	enum error{
		NONE,            //default
		MAX_CONNECTIONS, //maximum allowed connections reached
		FAILED           //failed to connect
	};
	error Error;

	/*
	This is needed if the host resolves to multiple IPs. This is used to try
	the next IP in the list until connection happens or we run out of IPs. 
	However, if the first IP is connected to, no other IP's will be connected
	to.
	*/
	boost::shared_ptr<wrapper::info> Info;

	/*
	This is typedef'd to socket in network::. The purpose of this class is to
	limit what data the call back has access to. For example the call back
	should not have access to any flag but the disconnect flag.
	*/
	class socket_data_visible
	{
	public:
		socket_data_visible(
			const int socket_FD_in,
			const std::string & host_in,
			const std::string & IP_in,
			const std::string & port_in,
			bool & disconnect_flag_in,
			buffer & recv_buff_in,
			buffer & send_buff_in,
			const error & Error_in
		):
			socket_FD(socket_FD_in),
			host(host_in),
			IP(IP_in),
			port(port_in),
			disconnect_flag(disconnect_flag_in),
			recv_buff(recv_buff_in),
			send_buff(send_buff_in),
			Error(Error_in)
		{}

		//const references to save space, non-const references may be changed
		const int socket_FD;      //WARNING: do not write this
		const std::string & host; //empty when incoming connection
		const std::string & IP;   //IP address host resolved to
		const std::string & port; //port on remote end
		buffer & recv_buff;
		buffer & send_buff;
		bool & disconnect_flag;   //trigger disconnect when set to true
		const error & Error;

		/*
		These must be set in the connect call back or the program will be
		terminated.
		*/
		boost::function<void (socket_data_visible &)> recv_call_back;
		boost::function<void (socket_data_visible &)> send_call_back;
	};

	socket_data_visible Socket_Data_Visible;
};
}//end of network namespace
#endif
