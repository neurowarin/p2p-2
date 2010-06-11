#ifndef H_THREAD_POOL
#define H_THREAD_POOL

//include
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>

//standard
#include <map>
#include <queue>
#include <set>

/*
This thread pool allows objects to be associated with jobs. The following
example creates a list of objects and starts jobs in each object. The dtor does
a join on the object so the job won't try to run on a destroyed object.

class test : private boost::noncopyable
{
public:
	test(thread_pool & TP_in):
		TP(TP_in)
	{
		//be careful about temporary objects
		TP.enqueue(boost::bind(&test::job, this), this);
	}
	~test()
	{
		//don't allow object to be destroyed when there are still jobs for it
		TP.join(this);
	}
private:
	void job()
	{
		boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
		std::cout << "job finished\n";
	}
	thread_pool & TP;
};

int main()
{
	thread_pool TP(8);
	std::list<boost::shared_ptr<test> > Test;
	for(int x=0; x<8; ++x){
		Test.push_back(boost::shared_ptr<test>(new test(TP)));
	}
}
*/
class thread_pool : private boost::noncopyable
{
	//accuracy of timer, may be up to this many ms off
	static const unsigned timer_accuracy_ms = 10;

public:
	thread_pool(const unsigned threads = boost::thread::hardware_concurrency()):
		stopped(false),
		timeout_ms(0)
	{
		for(unsigned x=0; x<threads; ++x){
			workers.create_thread(boost::bind(&thread_pool::dispatcher, this));
		}
	}

	~thread_pool()
	{
		//wait for existing jobs to finish
		join();

		//stop threads
		workers.interrupt_all();
		workers.join_all();
		timeout_thread.interrupt();
		timeout_thread.join();
	}

	//clear jobs
	void clear(void * object_ptr = NULL)
	{
		boost::mutex::scoped_lock lock(mutex);
		job_queue.clear();
		timeout_jobs.clear();
	}

	//enqueue job
	bool enqueue(const boost::function<void ()> & func, void * object_ptr = NULL)
	{
		boost::mutex::scoped_lock lock(mutex);
		if(stopped){
			return false;
		}else{
			job_queue.push_back(job(func, object_ptr));
			job_objects.insert(object_ptr);
			job_cond.notify_one();
			return true;
		}
	}

	//enqueue job after specified delay (ms)
	bool enqueue(const boost::function<void ()> & func, const unsigned delay_ms,
		void * object_ptr = NULL)
	{
		boost::mutex::scoped_lock lock(mutex);
		if(stopped){
			return false;
		}else{
			if(timeout_thread.get_id() == boost::thread::id()){
				//lazy start timeout_thread
				timeout_thread = boost::thread(boost::bind(&thread_pool::timeout_dispatcher, this));
			}
			timeout_jobs.insert(std::make_pair(timeout_ms + delay_ms, job(func, object_ptr)));
			job_objects.insert(object_ptr);
			return true;
		}
	}

	//block until all jobs done
	void join(void * object_ptr = NULL)
	{
		boost::mutex::scoped_lock lock(mutex);
		join_priv(object_ptr);
	}

	//enable enqueue
	void start()
	{
		boost::mutex::scoped_lock lock(mutex);
		stopped = false;
	}

	//disable enqueue
	void stop()
	{
		stopped = true;
	}

private:
	class job
	{
	public:
		job(){}
		job(const boost::function<void ()> & func_in, void * object_ptr_in):
			func(func_in),
			object_ptr(object_ptr_in)
		{}
		job(const job & J):
			func(J.func),
			object_ptr(J.object_ptr)
		{}
		boost::function<void ()> func;
		void * object_ptr;
	};

	boost::mutex mutex;                      //locks everyting
	boost::thread_group workers;             //worker threads
	boost::condition_variable_any job_cond;  //cond notified when job added
	std::deque<job> job_queue;               //jobs to run
	std::multiset<void *> job_objects;       //all object_ptrs in job_queue
	boost::condition_variable_any join_cond; //cond used for join
	bool stopped;                            //when true no jobs can be added
	boost::thread timeout_thread;            //timout_dispatcher() thread
	boost::uint64_t timeout_ms;              //incremented by timeout_thread

	//jobs enqueued after timeout, when timeout_ms >= key timeout expired
	std::multimap<boost::uint64_t, job> timeout_jobs;

	//consumes enqueued jobs, does call backs
	void dispatcher()
	{
		job tmp;
		while(true){
			{//begin lock scope
			boost::mutex::scoped_lock lock(mutex);
			while(job_queue.empty()){
				job_cond.wait(mutex);
			}
			tmp = job_queue.front();
			job_queue.pop_front();
			}//end lock scope
			tmp.func();
			{//begin lock scope
			boost::mutex::scoped_lock lock(mutex);
			std::multiset<void *>::iterator it = job_objects.find(tmp.object_ptr);
			if(it != job_objects.end()){
				job_objects.erase(it);
			}
			}//end lock scope
			join_cond.notify_all();
		}
	}

	/*
	Performs join. If object_ptr = NULL join performed on all objects.
	Precondition: mutex locked.
	*/
	void join_priv(void * object_ptr)
	{
		if(object_ptr == NULL){
			//join on all
			while(!job_objects.empty()){
				join_cond.wait(mutex);
			}
		}else{
			//join on jobs from specific object
			while(job_objects.find(object_ptr) != job_objects.end()){
				join_cond.wait(mutex);
			}
		}
	}

	//enqueues jobs after a timeout (ms)
	void timeout_dispatcher()
	{
		while(true){
			boost::this_thread::sleep(boost::posix_time::milliseconds(timer_accuracy_ms));
			{//BEGIN lock scope
			boost::mutex::scoped_lock lock(mutex);
			timeout_ms += timer_accuracy_ms;
			while(!timeout_jobs.empty() && timeout_ms >= timeout_jobs.begin()->first){
				job_queue.push_back(timeout_jobs.begin()->second);
				job_cond.notify_one();
				timeout_jobs.erase(timeout_jobs.begin());
			}
			}//END lock scope
		}
	}
};
#endif
