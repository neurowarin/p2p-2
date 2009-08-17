//SINGLETON, THREADSAFE, THREAD SPAWNING
#ifndef H_SHARE
#define H_SHARE

//custom
#include "database.hpp"
#include "hash_tree.hpp"
#include "path.hpp"
#include "settings.hpp"
#include "share_pipeline_2_write.hpp"

//include
#include <atomic_int.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>
#include <boost/utility.hpp>
#include <portable_sleep.hpp>

//standard
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

class share : private boost::noncopyable
{
public:
	share();
	~share();

	/*
	start:
		Starts share indexing threads.
		Note: This is only to be used from singleton_start.
	stop:
		Stops share indexing threads.
		Note: This is only to be used from singleton_stop.
	*/
	void start();
	void stop();

	/*
	size_bytes:
		Returns size of shared files.
	size_files:
		Returns the number of shared files.
	*/
	boost::uint64_t size_bytes();
	boost::uint64_t size_files();

private:
	share_pipeline_2_write Share_Pipeline_2_Write;
};
#endif

