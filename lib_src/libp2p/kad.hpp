#ifndef H_KAD
#define H_KAD

//custom
#include "db_all.hpp"
#include "exchange_udp.hpp"
#include "k_bucket.hpp"
#include "k_find.hpp"
#include "k_func.hpp"
#include "k_route_table.hpp"

//include
#include <bit_field.hpp>
#include <net/net.hpp>

class kad
{
public:
	kad();
	~kad();

	/*
	find_node_add:
		Find a node ID. The call back is used when the node ID is found.
	*/
	void find_node_add(const std::string & ID_to_find,
		const boost::function<void (const net::endpoint &, const std::string &)> & call_back);
	void find_node_remove(const std::string & ID);

private:
	boost::thread network_thread;
	exchange_udp Exchange;
	const std::string local_ID;
	k_route_table Route_Table;
	k_find Find;

	/*
	Function call proxy. A thread adds a function object and network_thread makes
	the function call. This eliminates a lot of locking.
	*/
	boost::mutex relay_job_mutex;
	std::deque<boost::function<void ()> > relay_job;

	/*
	find_node_relay:
		Called as relay job. Starts the find process for a node ID.
	network_loop:
		Loop to handle timed events and network events.
	process_relay_job:
		Called by network_thread to process relay jobs.
	*/
	void find_node_add_relay(const std::string ID_to_find,
		const boost::function<void (const net::endpoint &, const std::string &)> call_back);
	void find_node_remove_relay(const std::string ID);
	void network_loop();
	void process_relay_job();

	/* Timed Functions
	Called once per second by network_thread.
	send_find_node:
		Send find_node messages.
	send_ping:
		Send ping messages.
	*/
	void send_find_node();
	void send_ping();

	/* Receive Call Back
	Functions named after the message they receive.
	*/
	void recv_find_node(const net::endpoint & from,
		const net::buffer & random, const std::string & remote_ID,
		const std::string & ID_to_find);
	void recv_host_list(const net::endpoint & from,
		const std::string & remote_ID, const std::list<net::endpoint> & hosts,
		const std::string ID_to_find);
	void recv_ping(const net::endpoint & from, const net::buffer & random,
		const std::string & remote_ID);
	void recv_pong(const net::endpoint & from, const std::string & remote_ID);
};
#endif
