#ifndef H_NET_SELECT
#define H_NET_SELECT

//custom
#include "listener.hpp"
#include "nstream.hpp"

//standard
#include <set>

namespace net{
class select
{
public:
	select();
	~select();

	//causes a thread blocked on operator () to return immediately
	void interrupt();

	/*
	Works like the select() system call. The read and write sets contain sockets
	to monitor. When the call returns the read set contains sockets that need to
	read and the write set contains sockets ready to write. If a timeout not
	specified then block until activity.
	*/
	void operator () (std::set<int> & read, std::set<int> & write, int timeout_ms = -1);
private:
	//self-pipe
	boost::shared_ptr<nstream> sp_read;
	boost::shared_ptr<nstream> sp_write;
};
}//end namespace net
#endif
