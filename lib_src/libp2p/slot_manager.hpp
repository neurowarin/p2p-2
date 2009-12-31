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
		boost::function<void (boost::shared_ptr<message::base>)> send_in,
		boost::function<void (boost::shared_ptr<message::base>)> expect_response_in,
		boost::function<void (boost::shared_ptr<message::base>)> expect_anytime_in,
		boost::function<void (network::buffer)> expect_anytime_erase_in
	);

	const int connection_ID;

	/*
	resume:
		When the peer_ID is received this function is called to resume downloads.
	*/
	void resume(const std::string & peer_ID);

private:
	/* See documentation for corresponding functions in connection.
	send:
		Pointer to connection::send.
	expect:
		Pointer to connection::expect.
	expect_anytime:
		Pointer to connection::expect_anytime.
	*/
	boost::function<void (boost::shared_ptr<message::base>)> send;
	boost::function<void (boost::shared_ptr<message::base>)> expect_response;
	boost::function<void (boost::shared_ptr<message::base>)> expect_anytime;
	boost::function<void (network::buffer)> expect_anytime_erase;

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
	make_block_requests:
		Makes any hash_tree of file block requests that need to be done.
	make_slot_requests:
		Does pending slot requests.
	recv_file_block:
		Call back for receiving a file block.
	recv_hash_tree_block:
		Call back for receiving a hash tree block.
	recv_request_block_failed:
		Call back for when a hash tree block request or file block request fails.
	recv_request_slot:
		Handles incoming request_slot messages.
	recv_request_slot_failed:
		Handles an ERROR in response to a request_slot.
	recv_slot:
		Handles incoming slot messages. The hash parameter is the hash of the file
		requested.
	*/
	void make_block_requests(boost::shared_ptr<slot> S);
	void make_slot_requests();
	bool recv_file_block(boost::shared_ptr<message::base> M,
		const boost::uint64_t block_num);
	bool recv_hash_tree_block(boost::shared_ptr<message::base> M,
		const boost::uint64_t block_num);
	bool recv_request_block_failed(boost::shared_ptr<message::base> M);
	bool recv_request_slot(boost::shared_ptr<message::base> M);
	bool recv_request_slot_failed(boost::shared_ptr<message::base> M);
	bool recv_slot(boost::shared_ptr<message::base> M, const std::string hash);
};
#endif
