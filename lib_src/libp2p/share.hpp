//SINGLETON, THREADSAFE, THREAD SPAWNING
#ifndef H_SHARE
#define H_SHARE

//boost
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>
#include <boost/utility.hpp>

//custom
#include "database.hpp"
#include "hash_tree.hpp"
#include "path.hpp"
#include "settings.hpp"
#include "share_pipeline_2_write.hpp"

//include
#include <atomic_int.hpp>
#include <portable_sleep.hpp>
#include <singleton.hpp>
#include <thread_pool.hpp>

//std
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

class share : public singleton_base<share>
{
	friend class singleton_base<share>;
public:
	/*
	share_size:
		Returns size of shared files. (bytes)
	stop:
		Stops all worker threads in preparation for program shut down.
	*/
	boost::uint64_t share_size();
	void stop();

private:
	share();

	/*
	remove_temporary_files:
		Remove all temporary files used for generating hash trees by the different
		worker threads.
	*/
	void remove_temporary_files();

	share_pipeline_2_write Share_Pipeline_2_Write;
};
#endif

