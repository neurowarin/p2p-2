#ifndef H_DB_BLACKLIST
#define H_DB_BLACKLIST

//boost
#include <boost/thread/mutex.hpp>

//custom
#include "atomic_int.h"
#include "global.h"

//sqlite
#include <sqlite3.h>

//std
#include <iostream>
#include <sstream>
#include <sstream>
#include <string>

class DB_blacklist
{
public:
	/*
	Add a host to the blacklist.

	Everytime a IP is added the modified_count is incremented. This is used by
	modified() to communicate back to objects if the blacklist has been added to.
	*/
	static void add(const std::string & IP)
	{
		init();
		DB_Blacklist->add_priv(IP);
	}

	//returns true if the IP is blacklisted
	static bool is_blacklisted(const std::string & IP)
	{
		init();
		return DB_Blacklist->is_blacklisted_priv(IP);
	}

	/*
	This function is used to communicate back to the caller whether or not the
	blacklist was modified since last checked.

	This function should be passed the same integer each time calling it.

	This function returns true if the last state seen by the caller is different
	from the current state of the blacklist.

	If this function should return true the first time it is called the
	last_state_seen should be initialized to non-zero.
	*/
	static bool modified(int & last_state_seen)
	{
		init();
		if(last_state_seen == DB_Blacklist->blacklist_state){
			//blacklist has not been updated
			return false;
		}else{
			//blacklist updated
			last_state_seen = DB_Blacklist->blacklist_state;
			return true;
		}
	}

private:
	//only DB_blacklist can initialize itself
	DB_blacklist();

	//init() must be called at the top of every public function
	static void init()
	{
		boost::mutex::scoped_lock lock(init_mutex);
		if(DB_Blacklist == NULL){
			DB_Blacklist = new DB_blacklist();
		}
	}

	static int is_blacklisted_call_back(void * object_ptr, int columns_retrieved, char ** query_response, char ** column_name)
	{
		*((bool *)object_ptr) = true;
		return 0;
	}

	//the one possible instance of DB_blacklist
	static DB_blacklist * DB_Blacklist;
	static boost::mutex init_mutex;

	/*
	This count starts at 1 and is incremented every time a server is added to the
	blacklist.
	*/
	atomic_int<int> blacklist_state;

	sqlite3 * sqlite3_DB;
	boost::mutex Mutex;     //mutex for functions called by public static functions

	/*
	All these functions correspond to public static functions.
	*/
	void add_priv(const std::string & IP);
	bool is_blacklisted_priv(const std::string & IP);
};
#endif
