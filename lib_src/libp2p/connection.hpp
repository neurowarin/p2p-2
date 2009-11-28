#ifndef H_CONNECTION
#define H_CONNECTION

//custom
#include "encryption.hpp"
#include "exchange.hpp"
#include "share.hpp"
#include "slot_manager.hpp"

//include
#include <boost/utility.hpp>
#include <network/network.hpp>

class connection : private boost::noncopyable
{
public:
	connection(
		network::proactor & Proactor_in,
		network::connection_info & CI
	);

	const std::string IP;

	//void open(const std::string & hash);

private:
	/*
	Locks all access to any data members. This lock is used for call backs as
	well as public functions.
	*/
	boost::recursive_mutex Recursive_Mutex;

	network::proactor & Proactor;
	encryption Encryption;
	exchange Exchange;
	slot_manager Slot_Manager;

	//allows us to determine if the blacklist has changed
	int blacklist_state;

	/* Proactor Call Backs
	key_exchange_recv_call_back:
		The recv call back used during key exchange.
	recv_call_back:
		The recv call back used after key exchange complete.
	send_call_back:
		The send call back used after key exchange complete.
	*/
	void key_exchange_recv_call_back(network::connection_info & CI);
	void recv_call_back(network::connection_info & CI);
	void send_call_back(network::connection_info & CI);
};
#endif
