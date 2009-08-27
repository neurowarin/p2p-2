/*
The proactor gets jobs from a reactor and dispacthes them by calling the
appropriate call back. The proactor uses as many threads as there are CPUs.
*/
#ifndef H_NETWORK_PROACTOR
#define H_NETWORK_PROACTOR

//include
#include <boost/utility.hpp>
#include <logger.hpp>
#include <network/network.hpp>

namespace network{

//predeclaration for PIMPL
class reactor;

class proactor
{
public:
	proactor(
		const std::string & port,
		boost::function<void (sock & Sock)> connect_call_back_in,
		boost::function<void (sock & Sock)> disconnect_call_back_in,
		boost::function<void (sock & Sock)> failed_connect_call_back_in
	);
	~proactor();

	/*
	Does asynchronous DNS resolution. Hands off DNS resolution to a thread inside
	the proactor which does DNS resolution, then calls reactor::connect.
	*/
	void connect(const std::string & host, const std::string & port);

//DEBUG, move documentation here
	/*

	*/
	unsigned connections();
	unsigned incoming_connections();
	unsigned max_incoming_connections();
	unsigned outgoing_connections();
	unsigned max_outgoing_connections();
	void max_connections(const unsigned max_incoming_connections_in,
		const unsigned max_outgoing_connections_in);
	unsigned max_connections_supported();
	unsigned current_download_rate();
	unsigned current_upload_rate();
	unsigned max_download_rate();
	unsigned max_upload_rate();
	void max_download_rate(const unsigned rate);
	void max_upload_rate(const unsigned rate);

private:
	/*
	Reactor which the proactor uses. This object is public and can be used to get
	network information, set rate limits, etc.
	*/
	boost::shared_ptr<reactor> Reactor;

	//worker threads to handle call backs
	boost::thread_group Workers;

	//call backs used by dispatch thread
	boost::function<void (sock & Sock)> failed_connect_call_back;
	boost::function<void (sock & Sock)> connect_call_back;
	boost::function<void (sock & Sock)> disconnect_call_back;

	/*
	Resolve jobs queue'd by resolve().
	Note: job_mutex locks all access to job container.
	Note: job_cond is used to notify resolve thread when there is work.
	std::pair<host, IP>
	*/
	boost::mutex resolve_job_mutex;
	boost::condition_variable_any resolve_job_cond;
	std::deque<std::pair<std::string, std::string> > resolve_job;

	/*
	dispatch:
		Dispatches worker threads (does call backs) to handle jobs from the
		reactor.
	resolve:
		Does DNS resolution.
	*/
	void dispatch();
	void resolve();
};
}
#endif
