#ifndef H_EXCHANGE
#define H_EXCHANGE

//custom
#include "encryption.hpp"
#include "message.hpp"

//include
#include <boost/shared_ptr.hpp>
#include <network/network.hpp>

class exchange : private boost::noncopyable
{
public:
	exchange(
		boost::mutex & Mutex_in,
		network::proactor & Proactor_in,
		network::connection_info & CI
	);

	//our connection ID with proactor
	const int connection_ID;

	/*
	recv_call_back:
		The proactor calls back to this function whenever data is received.
		Note: Only connection should use this.
	*/
	void recv_call_back(network::connection_info & CI);

	/*
	expect_response:
		After sending a message that expects a response this function should be
		called with the message expected. If there are multiple possible messages
		expected a composite message should be used.
	expect_anytime:
		Expect a incoming message at any time.
	expect_anytime_remove:
		Removes messages expected anytime that expects the specified buffer.
	send:
		Sends a message. Handles encryption.
	*/
	void expect_response(boost::shared_ptr<message::base> M);
	void expect_anytime(boost::shared_ptr<message::base> M);
	void expect_anytime_erase(network::buffer buf);
	void send(boost::shared_ptr<message::base> M);

private:
	boost::mutex & Mutex;
	network::proactor & Proactor;

	/*
	Expected responses are pushed on the back of Expect after a request is sent.
	When a response arrives it is for the message on the front of Expect.
	*/
	std::list<boost::shared_ptr<message::base> > Expect_Response;

	/*
	Incoming messages that aren't responses are processed by the messages in this
	container. The message objects in this container are reused.
	*/
	std::list<boost::shared_ptr<message::base> > Expect_Anytime;

	//key exchange and stream cypher
	encryption Encryption;

	/*
	Before the key exchange is done the exchange::send() function may receive
	messages that need to be encrypted. When this happens those messages are
	stored here and sent FIFO when Encryption.ready() = true.
	*/
	std::list<boost::shared_ptr<message::base> > Encrypt_Buf;

	//state of blacklist (used as hint to see if we need to check)
	int blacklist_state;

	/* Functions to handle receiving messages.
	recv_p_rA:
		Call back to receive p and rA (see protocol documentation).
	recv_rB:
		Call back to receive rB (see protocol documentation).
	send_buffered:
		Sends messages buffered until key exchange complete.
	*/
	bool recv_p_rA(boost::shared_ptr<message::base> M, network::connection_info & CI);
	bool recv_rB(boost::shared_ptr<message::base> M, network::connection_info & CI);
	void send_buffered();
};
#endif
