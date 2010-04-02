#ifndef H_NETWORK_DISPATCHER
#define H_NETWORK_DISPATCHER

//custom
#include "connection_info.hpp"

//include
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

//standard
#include <list>

namespace network{
class dispatcher : private boost::noncopyable
{
	//number of threads to use to do call backs
	static const unsigned dispatcher_threads = 4;
public:
	dispatcher(
		const boost::function<void (connection_info &)> & connect_call_back_in,
		const boost::function<void (connection_info &)> & disconnect_call_back_in
	):
		connect_call_back(connect_call_back_in),
		disconnect_call_back(disconnect_call_back_in)
	{

	}

	/*
	Schedule call back for connect.
	Precondition: No other call back must have been scheduled before the connect
		call back.
	*/
	void connect(const boost::shared_ptr<connection_info> & CI)
	{
		boost::mutex::scoped_lock lock(job_mutex);
		job.push_front(std::make_pair(CI->connection_ID, boost::bind(
			&dispatcher::connect_call_back_wrapper, this, CI)));
		job_cond.notify_one();
	}

	/*
	Schedule call back for disconnect, or failed to connect. If scheduling a job
	for a disconnect then a connect call back must have been scheduled first.
	Postcondition: No other jobs for the connection should be scheduled.
	*/
	void disconnect(const boost::shared_ptr<connection_info> & CI)
	{
		boost::mutex::scoped_lock lock(job_mutex);
		job.push_back(std::make_pair(CI->connection_ID, boost::bind(
			&dispatcher::disconnect_call_back_wrapper, this, CI)));
		job_cond.notify_one();
	}

	/*
	Schedule call back for recv.
	Precondition: The connect call back must have been scheduled.
	*/
	void recv(const boost::shared_ptr<connection_info> & CI,
		const boost::shared_ptr<buffer> & recv_buf)
	{
		boost::mutex::scoped_lock lock(job_mutex);
		job.push_back(std::make_pair(CI->connection_ID, boost::bind(
			&dispatcher::recv_call_back_wrapper, this, CI, recv_buf)));
		job_cond.notify_one();
	}

	/*
	Schedule call back for send_buf size decrease.
	Precondition: The connect call back must have been scheduled.
	*/
	void send(const boost::shared_ptr<connection_info> & CI, const unsigned latest_send,
		const unsigned send_buf_size)
	{
		boost::mutex::scoped_lock lock(job_mutex);
		job.push_back(std::make_pair(CI->connection_ID, boost::bind(
			&dispatcher::send_call_back_wrapper, this, CI, latest_send,
			send_buf_size)));
		job_cond.notify_one();
	}

	//start dispatcher
	void start()
	{
		for(int x=0; x<dispatcher_threads; ++x){
			workers.create_thread(boost::bind(&dispatcher::dispatch, this));
		}
	}

	//stop dispatcher
	void stop()
	{
		workers.interrupt_all();
		workers.join_all();
		job.clear();
		memoize.clear();
	}

private:
	boost::thread_group workers;

	const boost::function<void (connection_info &)> connect_call_back;
	const boost::function<void (connection_info &)> disconnect_call_back;

	/*
	Call backs to be done by dispatch(). The connect_job container holds connect
	call back jobs. The other_job container holds send call back and disconnect
	call back jobs.

	There are some invariants to the call back system.
	1. Only one call back for a socket is done concurrently.
	2. The first call back for a socket must be the connect call back.
	3. The last call back for a socket must be the disconnect call back.

	The memoize system takes care of invariant 1.
	Invariant 2 is insured by always pushing connect jobs on the the front of the
		job container.
	Invariant 3 is insured by requiring that disconnect jobs be scheduled last.
	*/
	boost::mutex job_mutex; //locks everything in this section
	boost::condition_variable_any job_cond;
	std::list<std::pair<int, boost::function<void ()> > > job;
	std::set<int> memoize;  //used to memoize connection_ID

	//worker threads which do call backs reside in this function
	void dispatch()
	{
		std::pair<int, boost::function<void ()> > tmp;
		while(true){
			{//begin lock scope
			boost::mutex::scoped_lock lock(job_mutex);
			while(true){
				for(std::list<std::pair<int, boost::function<void ()> > >::iterator
					iter_cur = job.begin(), iter_end = job.end(); iter_cur != iter_end;
					++iter_cur)
				{
					std::pair<std::set<int>::iterator, bool>
						ret = memoize.insert(iter_cur->first);
					if(ret.second){
						tmp = *iter_cur;
						job.erase(iter_cur);
						goto run_job;
					}
				}
				job_cond.wait(job_mutex);
			}
			}//end lock scope
			run_job:

			//run call back
			tmp.second();

			{//begin lock scope
			boost::mutex::scoped_lock lock(job_mutex);
			memoize.erase(tmp.first);
			}//end lock scope
			job_cond.notify_all();
		}
	}

	//wrapper for connect call back to dereference shared_ptr
	void connect_call_back_wrapper(boost::shared_ptr<connection_info> CI)
	{
		connect_call_back(*CI);
	}

	//wrapper for connect call back to dereference shared_ptr
	void disconnect_call_back_wrapper(boost::shared_ptr<connection_info> CI)
	{
		disconnect_call_back(*CI);
	}

	/*
	Wrapper for recv_call_back to dereference shared_ptr. Also, this function is
	necessary to allow recv_call_back to be changed while another recv call back
	is being blocked by in dispatch().
	*/
	void recv_call_back_wrapper(boost::shared_ptr<connection_info> CI, boost::shared_ptr<buffer> recv_buf)
	{
		if(CI->recv_call_back){
			CI->recv_buf.append(*recv_buf);
			CI->latest_recv = recv_buf->size();
			CI->recv_call_back(*CI);
		}
	}

	/*
	Wrapper for send_call_back to dereference shared_ptr. Also, this function is
	necessary to allow send_call_back to be changed while another send call back
	is being blocked by in dispatch().
	*/
	void send_call_back_wrapper(boost::shared_ptr<connection_info> CI,
		const unsigned latest_send, const int send_buf_size)
	{
		CI->latest_send = latest_send;
		CI->send_buf_size = send_buf_size;
		if(CI->send_call_back){
			CI->send_call_back(*CI);
		}
	}
};
}
#endif
