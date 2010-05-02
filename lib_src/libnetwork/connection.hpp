#ifndef H_NETWORK_CONNECTION
#define H_NETWORK_CONNECTION

//custom
#include "ID_manager.hpp"

//include
#include <network/endpoint.hpp>
#include <network/nstream.hpp>
#include <network/proactor.hpp>

//standard
#include <set>

namespace network{
class connection : private boost::noncopyable
{
private:
	/*
	Timeout (seconds) before idle sockets disconnected. This has to be quite high
	otherwise client that download very slowly will be erroneously disconnected.
	*/
	static const unsigned idle_timeout = 600;

	bool connected;          //false if async connect in progress
	ID_manager & ID_Manager; //allocates and deallocates connection_ID
	std::set<endpoint> E;    //endpoints to try to connect to
	std::time_t last_seen;   //last time there was activity on socket
	int socket_FD;           //file descriptor, will change after open_async()

public:
	/*
	Ctor for outgoing connection.
	Note: This ctor does DNS resolution if needed.
	*/
	connection(
		ID_manager & ID_Manager_in,
		const std::string & host_in,
		const std::string & port_in
	);

	//ctor for incoming connection
	connection(
		ID_manager & ID_Manager_in,
		const boost::shared_ptr<nstream> & N_in
	);

	~connection();

	//modified by proactor
	bool disc_on_empty; //if true we disconnect when send_buf is empty
	buffer send_buf;    //bytes to send appended to this

	//const
	const boost::shared_ptr<nstream> N; //connection object
	const std::string host;             //hostname or IP to connect to
	const std::string port;             //port number
	const int connection_ID;            //see above

	/* WARNING
	This should not be modified by any thread other than a dispatcher
	thread. Modifying this from the network_thread is not thread safe unless
	it hasn't yet been passed to dispatcher.
	*/
	boost::shared_ptr<connection_info> CI;

	/*
	half_open:
		Returns true if async connect in progress.
	is_open:
		Returns true if async connecte succeeded. This is called after half
		open socket becomes writeable.
		Postcondition: If true returned then half_open() will return false.
	open_async:
		Open async connection, returns false if couldn't allocate socket.
		Postcondition: socket_FD, and CI will change.
	socket:
		Returns file descriptor.
		Note: Will return different file descriptor after open_async().
	timed_out:
		Returns true if connection timed out.
	touch:
		Resets timer used for timeout.
	*/
	bool half_open();
	bool is_open();
	bool open_async();
	int socket();
	bool timed_out();
	void touch();
};
}//end namespace network
#endif
