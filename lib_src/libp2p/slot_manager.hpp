#ifndef H_SLOT_MANAGER
#define H_SLOT_MANAGER

//custom
#include "message.hpp"
#include "share.hpp"
#include "slot.hpp"

//include
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <convert.hpp>
#include <network/network.hpp>

class slot_manager : private boost::noncopyable
{
public:
	slot_manager(
		const int connection_ID_in,
		boost::function<void (boost::shared_ptr<message::base>)> expect_in,
		boost::function<void (boost::shared_ptr<message::base>)> send_in
	);

	const int connection_ID;

	/*
	recv_request_slot:
		Handles incoming request_slot messages.
	recv_request_slot_failed:
		Handles an ERROR in response to a request_slot.
	recv_slot:
		Handles incoming slot messages. The hash parameter is the hash of the file
		requested.
	resume:
		When the peer_ID is received this function is called to resume downloads.
	*/
	bool recv_request_slot(boost::shared_ptr<message::base> M);
	bool recv_request_slot_failed(boost::shared_ptr<message::base> M);
	bool recv_slot(boost::shared_ptr<message::base> M, const std::string hash);
	void resume(const std::string & peer_ID);

private:
	//pointers to connection::expect and connection::send
	boost::function<void (boost::shared_ptr<message::base>)> expect;
	boost::function<void (boost::shared_ptr<message::base>)> send;

	/*
	Outgoing_Slot holds slots we have opened with the remote host.
	Incoming_Slot container holds slots the remote host has opened with us.
	Note: Index in vector is slot ID.
	Note: If there is an empty shared_ptr it means the slot ID is available.
	*/
	std::map<unsigned char, boost::shared_ptr<slot> > Outgoing_Slot;
	std::map<unsigned char, boost::shared_ptr<slot> > Incoming_Slot;

	/*
	These are passed to, and adjusted by, the transfer objects for adaptive
	pipeline sizing.
	Note: These are only for pipelining of block requests.
	*/
	unsigned pipeline_cur; //current size (unfulfilled requests
	unsigned pipeline_max; //current maximum size

	/*
	When making requests for blocks we loop through slots. This keeps track of
	the latest slot that had a block sent.
	*/
	unsigned char latest_slot;

	/*
	open_slots:
		Number of slots opened, or requested, from the remote host.
		Invariant: 0 <= open_slots <= 256.
	Pending:
		Hashes of files that need to be downloaded from host. Files will be
		queued up if the slot limit of 256 is reached.
	*/
	unsigned open_slots;
	std::queue<std::string> Pending;

	/*
	make_slot_requests:
		Does pending slot requests.
	*/
	void make_block_requests(boost::shared_ptr<slot> S);
	void make_slot_requests();
};
#endif
