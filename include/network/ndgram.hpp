#ifndef H_NETWORK_NDGRAM
#define H_NETWORK_NDGRAM

//custom
#include "protocol.hpp"
#include "system_include.hpp"

namespace network{
class ndgram_base
{
public:
	virtual ~ndgram_base(){}

	/*
	close:
		Close the socket.
	is_open:
		Returns true if can send/recv data.
	local_IP:
		Returns local port, or empty string if error.
		Postcondition: If error then close() called.
	local_port:
		Returns local port, or empty string if error.
		Postcondition: If error then close() called.
	open (no parameters):
		Opens socket for sending. Does not bind to a port.
	open (one parameter):
		Open for sending/receiving, endpoint is local. Example:
		Accept only from localhost. Choose random port to listen on.
			network::get_endpoint("localhost", "0", network::udp);
		Accept connections on all interfaces. Use port 1234.
			network::get_endpoint("", "1234", network::udp);
	recv:
		Receive data. Returns number of bytes received or 0 if the host
		disconnected. E is set to the endpoint of the host that sent the data. E
		may be empty if error.
	send:
		Writes bytes from buffer. Returns the number of bytes sent or 0 if the
		host disconnected. The sent bytes are erased from the buffer.
	socket:
		Returns socket file descriptor, or -1 if disconnected.
	*/
	virtual void close() = 0;
	virtual bool is_open() const = 0;
	virtual std::string local_IP() = 0;
	virtual std::string local_port() = 0;
	virtual void open() = 0;
	virtual void open(const endpoint & E) = 0;
	virtual int recv(network::buffer & buf, boost::shared_ptr<endpoint> & E) = 0;
	virtual int send(network::buffer & buf, const endpoint & E) = 0;
	virtual int socket() = 0;
};

class ndgram : public ndgram_base, private boost::noncopyable
{
	//max we attempt to send/recv in one go
	static const int MTU = 1536;
public:
	ndgram();                   //open but don't bind (used for send)
	ndgram(const endpoint & E); //open and bind (used for send/recv)
	~ndgram();

	void close();
	bool is_open() const;
	std::string local_IP();
	std::string local_port();
	void open();
	void open(const endpoint & E);
	int recv(network::buffer & buf, boost::shared_ptr<endpoint> & E);
	int send(network::buffer & buf, const endpoint & E);
	int socket();

private:
	//-1 if not connected, or >= 0 if connected
	int socket_FD;
};
}//end of namespace network
#endif
